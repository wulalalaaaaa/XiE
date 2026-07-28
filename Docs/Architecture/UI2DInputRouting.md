# UI2D 输入路由系统

## 范围与数据流

阶段 3E 的正式输入链路为：

```text
GLFW / Win32
  -> InputEventQueue（窗口逻辑坐标）
  -> BasicUIHitTester
  -> BasicUIInputRouter
  -> Preview / Target / Bubble
  -> UIEventListenerRegistry
  -> Hover / Press / Capture / Click
  -> UISceneMutationQueue
```

Router 不读取平台对象、不乘 DPI、不执行 Layout、Render 或 Present。Pointer 命中直接消费 `BasicUIHitTester` 的权威 target、route、矩阵和 Clip 结果。

## InputEvent 与 UIEvent

平台事件统一为 `PointerMoveEvent`、`PointerButtonEvent`、`PointerWheelEvent`、`PointerLeaveEvent`、`PointerCancelEvent`、`KeyEvent`、`TextInputEvent` 和 `WindowFocusEvent`。每个事件带 `WindowHandle`；Pointer 事件保留 `PointerId`，鼠标固定为 0；位置和点击阈值均使用窗口逻辑像素。

`UIEvent` 增加路由语义：事件类型、阶段、target、currentTarget、Scene/局部坐标、按钮、滚轮、按键、字符、修饰键、时间戳，以及 handled/defaultPrevented/传播控制状态。结构不保存 HWND、GLFWwindow 或后端对象。

## Preview、Target 与 Bubble

普通路由事件使用 HitTest route 或捕获目标的当前父链：

1. Preview：Root 到 Target，包含 Target 的 Preview 监听器。
2. Target：只调用 Target 的 Target 监听器。
3. Bubble：Target 到 Root，包含 Target 的 Bubble 监听器。

每经过一个节点都会重新设置 `currentTarget`，并使用该节点已有的 `sceneToLocal` 计算 `localPosition`。矩阵不可逆的 currentTarget 不调用监听器。结构 Mutation 只入队，所以当前 route 在整个 Dispatch 中稳定。

`handled` 不停止传播；`PreventDefault` 阻止自动 Capture、Press、Focus 和 Click；`StopPropagation` 停止后续节点；`StopImmediatePropagation` 同时停止当前节点剩余监听器。

## physicalTarget 与 dispatchTarget

`physicalTarget` 始终由当前坐标 HitTest 得到，用于 Hover、Enter/Leave、Wheel 和 Click 有效性判断。

Move、Down、Up、Cancel 的 `dispatchTarget` 优先采用当前 Pointer Capture；无 Capture 时才使用 physicalTarget。捕获期间移出节点后，Move/Up 仍到捕获节点，但 Hover 继续反映实际鼠标位置。

## Hover route diff

`UIHoverTracker` 按 PointerId 保存 Root 到物理目标的 route。更新时查找旧、新 route 的最长公共前缀：旧后缀按 Target 向祖先顺序发送 Leave，新后缀按公共祖先向 Target 顺序发送 Enter。Enter/Leave 是 Target 直接通知，不进行 Preview/Bubble。

PointerLeave 清空对应 Pointer 的完整 route；Focus Lost 清空全部 route。Hover 改变只标记 Visual Dirty，不修改 `UIVisual`，也不要求持续帧循环。

## Pointer Capture

`UIPointerCaptureService` 每窗口、每 PointerId 最多保存一个捕获节点。Primary Down 未 PreventDefault 且未显式处理 Capture 时，默认捕获 Down 的 dispatchTarget。监听器可显式捕获其他有效节点或释放捕获，显式操作优先。

PointerUp 和 PointerCancel 默认释放对应 Capture；Focus Lost、窗口隐藏、节点销毁、隐藏或禁用会清理 Capture。不同 PointerId 和不同 UIWindowRuntime 完全隔离。

## Press 与 Click

`UIPressTracker` 以 `(PointerId, PointerButton)` 为键保存 pressedNode、Down 坐标、时间和 canceled 状态。Down 未 PreventDefault 时建立 Press；移动距离超过 `UIClickPolicy::movementThreshold` 时仅将 Press 标记 canceled；Up、Cancel、Focus Lost 和节点失效结束或取消状态。

PointerUp 后，仅当 Press 有效、节点仍可交互、释放 physicalTarget 等于 pressedNode、移动未超阈值且 Up 未 PreventDefault 时合成一次 Click。Click 重新构建 pressedNode 当前父链，不复用 Down route。本阶段没有双击、长按、时间上限或拖拽系统。

## Wheel 与键盘

Wheel 忽略 Capture，发送给 physicalTarget；没有物理目标时发送给 Scene Root，并执行完整三阶段路由。

KeyDown、KeyUp 和 TextInput 仅在 OS Window Focus active 时派发；有效 Logical Focus 为目标，否则回退 Root。PointerDown 使用 physical route 查找最近 focusable 节点并排入默认焦点请求。Tab 在 KeyDown 未 PreventDefault 时调用 `BasicUIFocusManager` 的稳定候选导航。IME Composition 仍未实现。

## Window Focus Lost

Focus Lost 先路由 WindowFocusLost，随后为每个存在 Capture/Press 的 Pointer 派发一次 PointerCancel，发送 Hover Leave，取消所有 Press、释放所有 Capture，再通知 FocusManager 保存恢复候选并清除 active focus。清理后不会合成 Click，旧节点也不再接收 Key/TextInput。

## Listener Registry 生命周期

每个 UIWindowRuntime 私有持有 `UIEventListenerRegistry`。Listener Handle 使用 index+generation，旧 Handle 不能删除复用槽的新 Listener；同节点监听器保持注册顺序。Dispatch 中的注销在本次 Dispatch 结束后生效，新注册监听器不会加入当前 Dispatch。节点销毁时 Runtime 自动删除该节点全部监听器。

Registry 不为每次分发复制 `std::function`，也不存在全局 Listener 表。

## Mutation、失效与重入安全

`UIEventContext::EnqueueMutation` 是回调修改 Scene 结构的正式入口。Runtime 在 Dispatch 与 Listener 延迟操作完成后 Flush `UISceneMutationQueue`，然后重新 Layout。Router 通过 `dispatchInProgress` 明确拒绝同步递归 Dispatch。

节点销毁会立即清理 Listener、Hover、Press、Capture 并通知 Focus/Animator/Router。隐藏或禁用后 Listener 保留，但 Runtime sanitize 会清理交互状态；Reparent 保留 Capture/Press，下一事件和 Click 使用新父链。

## UIWindowRuntime 与调度

最终更新顺序为：窗口尺寸/DPI 失效、必要 Layout、输入 Dispatch、Mutation Flush、交互状态清理、必要再次 Layout、Animator 骨架、第二次 Mutation/Layout、RenderBuilder。

`UIFrameActivity::hasActiveInteraction` 在存在 Capture、有效 Press 或待处理输入时为 true。Hover 改变会触发一次 Visual Build/Render，之后恢复 Static；Hidden 清空交互并跳过 Dispatch、Build、Render、Present。

## 双窗口与当前限制

`UI2DInputTest` 使用相同 Scene 和合成输入分别驱动 GLFW/OpenGL 与 Win32 layered/software，并将各平台 `InputEventQueue` 直接绑定到对应 UIWindowRuntime。

当前未实现：Widget 默认行为、方向键空间导航、拖拽、双击、长按、手势、触摸平台适配、文本编辑、IME Composition、Theme 交互状态样式和动画。
