# XiE 2D 双运行时架构

## 定位

XiE 是共享窗口宿主与 `DrawList2D` 绘制协议、但保留两套领域运行时的 2D 引擎：`Runtime2D` 表达游戏世界，`UI2D` 表达窗口 UI 场景。二者不共享 Entity、节点、焦点或布局语义。

阶段 3A 已固定边界和生命周期，阶段 3B 已提供正式 `BasicLayoutEngine`。HitTest、事件路由、动画和节点绘制仍只有可替换接口或明确的 Null/Empty 实现。

## 正式分层

```text
Foundation
├─ Application
├─ Platform Contracts ── Platform GLFW / Platform Win32
├─ Input
├─ Renderer2D Contracts ── Resources ── OpenGL / Software
├─ Runtime2D
└─ UI2D (Foundation + Input + Renderer2D Contracts)

Projects
├─ test2Dgame: GLFW + OpenGL + Runtime2D
└─ DesktopSurfaceTest: Win32 + Software + DrawList2D
```

现有 `xie_engine` 是兼容共享库，仍承载旧 Core/Renderer/Assets/Story；新的 Application、Input、Renderer2D、Runtime2D、UI2D、Platform GLFW 与 Platform Win32 已物理拆成独立静态库。

## 两条执行链

游戏链：

```text
World2D → 项目侧 RenderSync2D → DrawList2D
→ Renderer2DFeature → OpenGLWindowRenderPipeline::Render
→ OpenGL2DRenderer → OpenGLSurface
```

兼容 `Core/Application` 仍在统一帧尾调用 `IRenderSurface::Present`，因此游戏路径只借助管线适配器执行 `Render`，不会在 Feature 内重复 Present。迁移到 `ApplicationRuntime` 后再由 `WindowContext::renderPipeline` 完整承担 Render/Present。

桌面链：

```text
DesktopSurfaceTest BuildDesktopDrawList → DrawList2D
→ LayeredWindowRenderPipeline → Software2DRenderer
→ LayeredWindowPresenter → UpdateLayeredWindow
```

`IWindowRenderPipeline` 统一 `Resize / Render / Present` 装配协议，不统一后端技术。透明窗口继续使用 BGRA8 premultiplied 软件合成。

## 共享边界

`DrawList2D` 是 Runtime2D、UI2D 与执行后端之间唯一正式绘制协议。DrawList 不拥有纹理；后端不拥有 Scene；UI2D 不执行 OpenGL、软件栅格化或 Present。

项目层选择宿主、窗口系统、资源库、渲染管线和领域 Runtime。Engine 不包含 EmoExec、数据库、Tag、Clipboard、托盘或快捷键业务。
