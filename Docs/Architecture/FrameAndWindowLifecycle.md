# 帧与窗口生命周期

## 创建和注册

```text
初始化 IApplicationHost / IWindowSystem
→ IWindowSystem::CreateWindow
→ 为该 WindowHandle 创建 InputEventQueue，并由具体平台窗口系统挂接（UI 窗口）
→ 创建 Surface、后端与 IWindowRenderPipeline
→ 创建项目 Runtime 或 UIWindowRuntime
→ 填充 WindowContext
→ WindowRuntimeRegistry::Register
```

项目负责最终装配。`ApplicationRuntime` 不创建 World2D、UIScene、数据库或具体后端。

## 主循环

```text
Poll platform events
→ execute ApplicationCommandQueue on main thread
→ refresh each WindowContext visibility/close/DPI
→ FrameScheduler::Evaluate
→ project update callback
→ project builds/submits DrawList2D
→ IWindowRenderPipeline::Render
→ ApplicationRuntime calls Present
→ wait when no window has work
```

`UIFrameActivity` 只暴露 pending input/mutation、layout/render、active animation。项目把这些布尔量映射到 `WindowContext::dirty`、FrameScheduler 的 animation 和 pending-command 参数；FrameScheduler 不读取 UIScene。

## UIWindowRuntime 固定更新顺序

1. Hidden 检查。
2. 消费窗口自己的 InputEventQueue。
3. 调用 InputRouter。
4. Flush 延迟 Scene Mutation。
5. 如果 Layout Dirty，调用 Layout Engine。
6. 更新 Animator。
7. 再次 Flush Mutation。
8. 如果 Visual/Transform/Children Dirty，调用 RenderBuilder。
9. 返回 `UI2DUpdateResult` 与 `UIFrameActivity`。

隐藏窗口不分发输入、不布局、不更新动画、不构建 DrawList、不请求 render。隐藏时 pointer move/button/wheel/key/text 被丢弃；最新 focus 生命周期事件最多保留一个，防止无限积累。重新显示会标记 Layout 与 Visual Dirty。

GLFW/Win32 的 `SetInputEventQueue` 是阶段 3A 的最小适配入口：项目把 `UIWindowRuntime::InputQueue()` 按窗口挂接，GLFW cursor 坐标直接使用逻辑坐标，Win32 client 坐标按窗口 DPI 转成逻辑像素。适配器只覆盖基础 pointer/key/text/focus 映射；完整键表与 IME 明确留待后续。该指针非拥有，窗口销毁前应解除挂接，或保证队列活到窗口销毁。

## 销毁

```text
close requested
→ unregister WindowContext
→ destroy UI/project runtime and Render Pipeline
→ destroy Surface/window-private framebuffer
→ IWindowSystem::DestroyWindow
→ exit after last window or explicit RequestExit
```

工作线程只能向 `ApplicationCommandQueue` 入队；命令在主循环统一执行。关闭队列后新命令被拒绝，未执行命令安全析构。本阶段不提供线程池。
