# XiE 2D Minimal Loop Architecture

## 1. 目标

本架构用于单人/小团队的 2D 最小闭环开发，目标是：

- 职责边界清晰，不做过度工程化。
- 支持运行时热更新（资源改动无需重启）。
- 支持输入移动、碰撞分离、渲染同步的完整链路。

## 2. 模块与职责

### 2.1 AssetRuntime2D（资产运行时）

路径：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/AssetRuntime2D.*`

职责：

- 统一加载与热更新：`main.xmesh / main.xsprite / main.xatlas / main.xtexture / main.xmat`
- 解析依赖链：`sprite -> atlas -> texture` 与 `material -> texture`
- 提供当前有效快照 `AssetRuntime2DSnapshot`

### 2.2 World2D（运行时世界状态）

路径：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/World2D.*`

职责：

- 保存实体运行时数据：`Transform2D / Velocity2D / Collider2D / SpriteRef2D`
- 管理实体 ID 与玩家实体引用
- 不处理文件、GPU、后端渲染

### 2.3 InputSystem2D（输入系统）

路径：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/InputSystem2D.*`

职责：

- 读取键盘输入（`WASD + 方向键`）
- 写入玩家实体速度向量（归一化方向 + 可调速度）

### 2.4 CollisionSystem2D（碰撞系统）

路径：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/CollisionSystem2D.*`

职责：

- 动态体 vs 静态体 AABB 检测
- 轴向最小分离（X/Y 取较小穿透轴）
- 分离后清零对应轴速度，避免持续穿透

### 2.5 RenderSync2D（渲染同步层）

路径：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/RenderSync2D.*`

职责：

- 从 `World2D + AssetRuntime2DSnapshot` 生成运行时提交网格
- 优先使用 `main.xmesh` 作为基础几何，并按实体碰撞盒缩放/平移实例化
- 回退策略：当 mesh 不可用时，使用内建四边形生成批次

### 2.6 Renderer2DFeature（渲染执行层）

路径：
`Engine/Renderer/Feature/Renderer2DFeature.*`

职责：

- 执行资产解析与后端提交
- 接收运行时覆盖网格（`SubmitRuntimeMesh/ClearRuntimeMesh`）
- 保持 `Renderer = Backend + Feature` 分层

## 3. 主循环顺序（已落地）

`HotReload -> Input -> Move -> Collision -> RenderSync -> Render`

对应位置：
`XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`

## 4. 运行时桥接关系

- `Application` 在初始化时将窗口句柄与渲染桥注入 `FrameContext`
- `GameApp` 每帧通过 `FrameContext` 访问：
  - `WindowHandle`（输入读取）
  - `RuntimeRender2D`（运行时网格提交）

相关文件：

- `Engine/Core/GameApp.h`
- `Engine/Core/Application.*`
- `Engine/Renderer/Renderer.*`

## 5. 最小闭环验收标准

1. 输入移动：玩家可由键盘控制移动。
2. 碰撞分离：玩家撞到静态障碍不穿透。
3. 热更新可见：运行中修改 `main.xmesh / main.xsprite / main.xatlas` 可见变化。
4. 职责边界清晰：资产、世界、输入、碰撞、渲染同步分离。

## 6. 后续扩展建议（保持简约）

- 在 `World2D` 增加 `Renderable2D`（图层、可见性、颜色覆写）而不引入完整 ECS。
- 碰撞系统可在当前分离基础上增加 swept 检测，优先覆盖高速体穿透场景。
- `AssetRuntime2D` 可补充失败状态缓存与 UI/日志汇总，但不改模块边界。

