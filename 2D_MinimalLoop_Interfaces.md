# XiE 2D Minimal Loop Interfaces

本文件定义当前 2D 最小闭环的关键接口、调用方式与约束。

## 1. FrameContext 与运行时渲染桥

文件：
`Engine/Core/GameApp.h`

```cpp
struct FrameContext {
    GLFWwindow* WindowHandle = nullptr;
    IRuntimeRender2D* RuntimeRender2D = nullptr;
};
```

```cpp
class IRuntimeRender2D {
public:
    virtual bool SubmitRuntimeMesh2D(
        const float* vertices, int vertexCount, int vertexDimension,
        const float* uvs, int uvCount,
        const unsigned int* indices, int indexCount) = 0;
    virtual void ClearRuntimeMesh2D() = 0;
};
```

约束：

- `vertexDimension` 仅支持 `2` 或 `3`
- `indices` 必须按三角形组织（`indexCount % 3 == 0`）
- `uvs` 为空时允许自动生成；不为空时 `uvCount == vertexCount`

## 2. GameApp 生命周期接口

文件：
`Engine/Core/GameApp.h`

```cpp
class IGameApp {
public:
    virtual void OnInit(const FrameContext& context) = 0;
    virtual void OnUpdate(float dt, const FrameContext& context) = 0;
    virtual void OnRender(const FrameContext& context) = 0;
    virtual void OnShutdown(const FrameContext& context) = 0;
};
```

约定：

- `OnUpdate` 承担 2D 运行时系统顺序调度。
- `OnRender` 当前为空实现（渲染提交已在 Update 的 RenderSync 完成）。

## 3. World2D 接口

文件：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/World2D.*`

```cpp
EntityId CreateEntity(const Entity2D& templateData = {});
Entity2D* FindEntity(EntityId id);
const Entity2D* FindEntity(EntityId id) const;
void SetPlayer(EntityId id);
EntityId GetPlayer() const;
std::vector<Entity2D>& Entities();
const std::vector<Entity2D>& Entities() const;
```

核心数据：

- `Transform2D::position`
- `Velocity2D::linear`
- `Collider2D::{halfExtent,isStatic}`
- `SpriteRef2D::spritePath`

## 4. InputSystem2D 接口

文件：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/InputSystem2D.*`

```cpp
void Tick(GLFWwindow* window, World2D& world) const;
void SetMoveSpeed(float unitsPerSecond);
```

行为：

- 读取 `WASD + 方向键`
- 更新玩家实体速度向量
- 对角移动方向会做归一化

## 5. CollisionSystem2D 接口

文件：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/CollisionSystem2D.*`

```cpp
void Solve(World2D& world) const;
```

行为：

- 遍历动态实体与静态实体
- AABB 重叠后按最小穿透轴分离
- 清零分离轴速度

## 6. AssetRuntime2D 接口

文件：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/AssetRuntime2D.*`

```cpp
void SetAssetRoot(const std::filesystem::path& assetRoot);
bool Initialize();
void TickHotReload();
const AssetRuntime2DSnapshot& GetSnapshot() const;
bool ChangedThisFrame() const;
```

`AssetRuntime2DSnapshot` 输出：

- `mesh/sprite/atlas/texture/material` 的当前句柄
- 对应解析后的有效路径
- `revision`（变更版本号）

## 7. RenderSync2D 接口

文件：
`XiE_Projects/test2Dgame/Source/App/Runtime2D/RenderSync2D.*`

```cpp
void Sync(
    const World2D& world,
    const AssetRuntime2DSnapshot& assets,
    Engine::IRuntimeRender2D* runtimeRender2D);
```

行为：

- 若有 `assets.mesh`，按实体变换生成批次并提交
- 若无 mesh，回退到内建四边形批次
- 当世界为空时调用 `ClearRuntimeMesh2D()`

## 8. 当前推荐调用顺序

在 `Test2DGameApp::OnUpdate` 中保持：

1. `m_Assets.TickHotReload()`
2. `m_InputSystem.Tick(...)`
3. 积分移动（`position += velocity * dt`）
4. `m_CollisionSystem.Solve(...)`
5. `m_RenderSync.Sync(...)`

## 9. 常见接入模板

```cpp
void GameApp::OnUpdate(float dt, const Engine::FrameContext& context) {
    assets.TickHotReload();
    input.Tick(context.WindowHandle, world);
    Integrate(world, dt);
    collision.Solve(world);
    renderSync.Sync(world, assets.GetSnapshot(), context.RuntimeRender2D);
}
```

