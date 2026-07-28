# UI2D 焦点管理

## 三种焦点状态

阶段 3F 明确区分：平台 `WindowFocusEvent` 表示 OS Window Focus；`BasicUIFocusManager` 保存 UI Logical Focus；键盘路由目标只在窗口 active 时取有效 Logical Focus，否则 active 窗口回退 Scene Root，inactive 窗口不派发普通 Key/TextInput。

每个 `UIWindowRuntime` 私有持有 `BasicUIFocusManager` 和 `UIFocusRequestQueue`，不存在跨窗口全局焦点状态。

## Focusable 与统一有效性

节点的 `UIFocusProperties` 包含 `focusable` 和 `tabIndex`。`tabIndex < 0` 允许 Pointer/程序化聚焦但不参加 Tab；非负值参加 Tab。

`IsFocusable(scene, node, focusRoot)` 是 Router、FocusManager 和测试共享的唯一判断：Handle 有效、focusable、local/effective visible、local/effective enabled，并位于 Focus Root 子树。opacity、hitTestVisible、Visual 类型和资源状态不参与判断。

## 请求队列

事件监听器和 `UIWindowRuntime::RequestFocus/ClearFocus` 只写入每窗口请求队列。优先级固定为 Lifecycle > Explicit > DefaultBehavior，同级最后一个有效请求生效。Router 在每个顶层 InputEvent 后 Flush，因此 PointerDown 产生的焦点能成为同批下一个 KeyEvent 的目标。

Focus 回调产生的新请求不进入当前 Flush，而在下一安全点提交；同步递归 Focus Dispatch 和 Input Dispatch 都被禁止。

## 焦点事件

普通切换顺序为：验证目标、FocusChanging、检查 PreventDefault、保存旧/新 route、提交 focusedNode、FocusLost、FocusGained、更新 Focus Within、标记 Visual Dirty。

三种事件均执行 Preview（Root 到 Target）、Target、Bubble（Target 到 Root）。`relatedTarget` 指向切换另一端，`focusReason` 保存 Pointer、KeyboardTab、Programmatic、Restore 或生命周期原因。普通 FocusChanging 可阻止；销毁、隐藏、禁用、窗口失焦和 Focus Root 排除造成的清理不可阻止。

## Pointer 默认聚焦

Primary PointerDown 路由完成后，若未 PreventDefault 且监听器没有显式请求，Router 从 `physicalTarget` 向 Root 查找最近的 focusable 节点并排入 DefaultBehavior 请求。显式请求优先。背景 Down 在策略允许时排入 ClearFocus。Capture 只改变事件 dispatchTarget，不改变默认聚焦使用的 physical route。

## Tab 顺序与候选缓存

候选必须 `IsFocusable` 且 `tabIndex >= 0`。先按 tabIndex 升序，再按父先子后、兄弟 insertionOrder 的稳定树遍历顺序；zOrder 和 PainterOrder 不参与。

Forward/Backward 支持无焦点起点、Shift+Tab 和可配置 wrap。KeyDown 先正常路由，未 PreventDefault 时才执行一次默认移动；repeat 每个输入事件独立移动一次。

`UIScene::FocusRevision` 在创建、销毁、Reparent、focusable、tabIndex、visible 和 enabled 变化时递增。FocusManager 仅在 revision 或 Focus Root 改变时重建候选，缓存不会保留失效 generation handle。

## Focus Route 与 Focus Within

Focus route 始终为 Scene Root 到 focusedNode。仅 focusedNode 的 `IsFocused` 为 true，route 中所有节点的 `HasFocusWithin` 为 true。切换时比较旧、新 route，只给 focused 节点和 FocusWithin 变化的祖先标记 Visual Dirty；状态不写入 `UIVisual`。

## Focus Root

默认 Focus Root 为 Scene Root，只允许聚焦其子树。切换 Root 时，当前焦点仍在新子树则保留并重建 route；否则选择新 Root 的首个 Tab 候选，无候选则清除。本阶段不实现 Focus Scope 栈或模态栈。

## 节点失效回退

销毁前保存 lastKnownParent/insertionOrder。focusedNode 失效时依次尝试最近有效 focusable 祖先、原 Tab 顺序下一个、原 Tab 顺序上一个，最后 ClearFocus。Hidden/Disabled 使用仍存在的父链；Reparent 后仍在 Root 内则保留并重建 route，移出 Root 则执行生命周期回退。

## Window Focus 与恢复

Window Focus Lost 先完成 Pointer Cancel/Capture/Press/Hover 清理，再保存 restoreCandidate、发送不可取消 FocusLost、清除 active focus route 并标记 Visual Dirty。inactive 窗口不向旧节点派发 Key/TextInput。

Window Focus Gained 在策略开启且候选仍有效时排入 Lifecycle Restore；候选失效或窗口 Hidden 时不随机选择替代节点。每个窗口的 restoreCandidate 独立。

## Runtime 与调度

Runtime 顺序为尺寸/DPI处理、必要预布局、逐顶层输入与逐事件焦点提交、Listener 延迟操作、Scene Mutation、节点失效/生命周期焦点提交、必要再次 Layout、Animator 骨架、RenderBuilder。

焦点变化通过 VisualRevision 触发一次 Build/Render，稳定后恢复 Static。焦点本身不保持持续 active；Hidden Runtime 不恢复焦点。

## 当前限制

当前没有 Enter/Space Widget 激活、方向键空间导航、Gamepad、Focus Scope 栈、模态系统、文本编辑、IME、Theme 状态解析或 Animator。`BasicUIRenderBuilder` 暂不根据 Focus Snapshot 自动改变样式。
