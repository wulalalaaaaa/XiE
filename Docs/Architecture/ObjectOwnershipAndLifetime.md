# 对象所有权与生命周期

## 应用级

- 平台装配代码拥有 `IApplicationHost` 与 `IWindowSystem` 的具体实例。
- `ApplicationRuntime` 非拥有地引用上述服务，拥有 `WindowRuntimeRegistry`、`ApplicationCommandQueue` 和 `FrameScheduler`。
- `WindowRuntimeRegistry` 按 `WindowHandle` generation 校验并拥有每个 `WindowContext`。
- 应用级资源宿主拥有 `TextureRegistry2D`、`ITextLayoutService` 和未来的 Theme Repository；窗口只引用它们。

`ApplicationRuntime` 关闭时先关闭命令队列，再销毁窗口上下文，最后请求 `IWindowSystem` 销毁原生窗口。退出应用前，平台服务必须仍然存活。

## 窗口级

`WindowContext` 是调度/装配记录，拥有：

- 可选 `IRenderSurface`；
- 可选 `IWindowRenderPipeline`；
- DPI、可见性、dirty、关闭请求与 FramePolicy。

字段顺序保证 Render Pipeline 先于 Surface 析构。管线可以非拥有地引用后端/Surface/Presenter；项目装配必须让这些依赖活得更久。

`UIWindowRuntime` 是一个 UI 窗口的领域运行时，独立拥有：

- `UIScene` 和 `UINodeStore`；
- `InputEventQueue`；
- `UISceneMutationQueue`；
- pointer capture 状态；
- Scene/窗口级 Dirty；
- `DrawList2D`；
- 当前不可变 `shared_ptr<const ResolvedUITheme>`。

`UI2DServices` 是显式非拥有引用。Layout 和 RenderBuilder 可无状态共享；FocusManager、InputRouter 与 Animator 是有状态服务，必须为每个窗口创建独立实例。禁止全局 Scene、FocusManager 或 Animator。

`BasicLayoutEngine` 不拥有 Scene、纹理或文本资源。它只保存可选的非拥有 `ITextureInfoProvider2D` 与 `ITextLayoutService` 指针，并在 Measure 时读取已经准备好的逻辑尺寸。资源宿主必须比 Layout Engine 活得更久；资源尺寸变化由资源协调层显式标记对应节点 Layout Dirty。

具体 GLFW/Win32 窗口系统只保存一个可选的、非拥有 `InputEventQueue*` 作为事件适配出口。队列由 `UIWindowRuntime` 拥有；项目装配负责在两者都存活时挂接，并在提前销毁 Runtime 时先解除挂接。平台 Contracts 不知道该队列，也不拥有 UI Runtime。

## UIScene 节点

`UINodeStore` 独占节点记录。句柄是 index + generation：销毁将 slot 标为空并增加 generation；旧句柄永远不能重新解析。节点只保存父子句柄，不保存跨生命周期裸指针。

每个 `UINodeRecord` 同时拥有三类互不覆盖的状态：

- `UITransform`：用户设计输入（position、size、pivot、scale、rotation）；
- `UILayoutParams`：尺寸规则、容器模式、margin/padding/spacing、anchor 和 alignment；
- `UILayoutState`：Layout Engine 产出的 desiredSize、arrangedRect、contentRect、sceneRect、矩阵、有效性和 effectiveOpacity/effectiveVisible/effectiveEnabled。

RenderBuilder 与下一阶段 HitTest 只能消费 `UILayoutState` 的最终结果，不重新解释 margin、anchor 或父节点坐标。

销毁规则：

1. 根节点不可销毁或重挂。
2. 销毁普通节点会先从父节点移除，再深度销毁整个子树。
3. 每个失效节点都会同步通知窗口的 FocusManager、Animator、InputRouter 与 pointer capture。
4. 重挂前沿父链检查，任何父子循环都会被拒绝。
5. 不存在或 generation 不匹配的节点操作返回失败，不抛异常、不崩溃。

当前 Dirty 实现把节点变化提升为 Scene 级重建，同时保留每个节点的 flags。后续可优化为子树级 Layout/Visual 重建而不改变接口。
