# UI2D 动画系统

## 边界与数据流

阶段 3G 的正式实现是每个 `UIWindowRuntime` 私有持有的 `BasicUIAnimator`。它只依赖 Foundation、UIScene、节点运行时属性和 Dirty 合同，不读取墙钟，不构建 DrawList，也不依赖 OpenGL、Software、Win32 或 GLFW。

属性分为四层：

```text
设计值（UITransform / opacity / UIVisual）
        +
布局值（UILayoutState.arrangedRect）
        +
动画覆盖（UIAnimatedOverrides）
        ↓
ResolveAnimatedProperties
        ↓
Transform / HitTest / RenderBuilder
```

Animator 从不把结果写回设计值、Theme、`UILayoutParams` 或 `UIVisual`。`UIAnimatedOverrides` 独立保存 position、visualSize、scale、rotation、opacity 和节点级 `colorMultiplier`；没有覆盖时统一回退到设计值或布局后的尺寸。

## 句柄、槽位与属性通道

`UIAnimationHandle` 是 index + generation 的强类型句柄。记录完成或取消时立即释放槽位并增加 generation，旧句柄随即失效。每个 Animator 使用独立 generation 域，因此句柄不能跨 `UIWindowRuntime` 使用。

活动索引使用 `(UINodeHandle, UIAnimatedProperty)` 属性通道。一个通道只有一个权威动画，并可带 FIFO 等待队列；每帧只遍历运行中和 Delay 中记录，暂停及排队记录不参与插值遍历。

## 值、From Current 与插值

`UIAnimationValue` 支持 `float`、`Vec2F`、`Color4f`。Position/VisualSize/Scale 必须使用向量，Rotation/Opacity 必须使用 float，ColorMultiplier 必须使用颜色；不匹配、非有限值、负 duration 和无效节点会在 Start 时拒绝。

`UIAnimationFromMode::Current` 在动画真正激活时读取统一最终属性。Queue 因而在出队时取值；Replace 会先捕获旧动画当前求值，再取消旧通道并以捕获值启动新动画，快速 Hover Enter/Leave 不会跳回端点。

Easing 是无状态纯函数，支持 Linear、Quad/Cubic 的 In/Out/InOut 和 SmoothStep。插值前后时间都限制在 `[0,1]`；Opacity、ColorMultiplier 限制到 `[0,1]`，VisualSize 非负，Rotation 使用弧度线性插值，Scale 允许负值和零值（零矩阵由 HitTest 的不可逆保护拒绝）。

## Delay、Fill、Loop 与方向

- 负或非有限 Delay 归一化为 0；负或非有限 Duration 拒绝。
- Delay 期间，Backwards/Both 写入播放方向对应的起点；None/Forwards 不新增覆盖。
- 完成时，Forwards/Both 保留最终覆盖但释放动画记录；None/Backwards 清除覆盖并恢复设计值。
- `iterationCount` 是总播放次数，`infinite` 忽略次数。
- Normal、Reverse、Alternate、AlternateReverse 在每个 iteration 上确定方向；Alternate 即 Yoyo。
- 大 delta 可跨多个 iteration；正式更新将异常或负 delta 视为 0，并把单帧极端 delta 限制为 86400 秒以保护数值状态。

## Replace、Reject 与 Queue

- Reject：通道已占用时创建失败。
- Replace：捕获当前值，取消活动动画及其等待队列，清理旧动画覆盖，再让新动画成为权威；旧 Completion 不执行，显式 Cancel 回调可执行。
- Queue：按创建顺序排队，前一条完成或取消后激活；等待期间不写属性，Explicit from 不自动继承上一条终值。

## Pause、Resume、Cancel 与回调

Pause 保留当前覆盖并把记录移出每帧活动列表；只有暂停动画时 `HasActiveAnimations=false`，窗口可回到 Static。Resume 从原 elapsed 继续。

Cancel 的 RestoreBase 清除通道覆盖，KeepCurrent 保留当前覆盖。还提供节点级和窗口级批量取消，以及独立 `ClearAnimatedOverride`。节点销毁会取消活动和排队记录、释放句柄、清除覆盖，不执行 Completion。

Completion/Cancel 回调在动画遍历之后执行。回调期间的 Start/Cancel 进入 Animator 延迟操作队列，递归 Update 被明确拒绝；需要修改节点树的回调必须写入 `UISceneMutationQueue`，由 `UIWindowRuntime` 在动画阶段后安全 Flush。

## Dirty 分类

| 属性 | Layout | Transform | Visual | HitTest |
|---|---:|---:|---:|---:|
| Position |  | ✓ | ✓ | ✓ |
| Scale |  | ✓ | ✓ | ✓ |
| Rotation |  | ✓ | ✓ | ✓ |
| Opacity |  |  | ✓ |  |
| ColorMultiplier |  |  | ✓ |  |
| VisualSize |  | ✓ | ✓ | ✓ |

Position 是布局后的视觉偏移，不重新 Measure。VisualSize 改变本地绘制/命中尺寸和矩阵 pivot，但永不参与 Measure/Arrange；本阶段没有 Layout Size 动画。

## Layout、HitTest 与 Render 一致性

Runtime 在布局后和动画后统一执行动画 Transform 求值。父矩阵、动画 Position/Scale/Rotation、动画尺寸 pivot、Scene AABB、逆矩阵和父子 effectiveOpacity 在同一阶段更新。HitTest 与 RenderBuilder 都通过 `ResolveAnimatedProperties` 读取本地尺寸；RenderBuilder 将节点 `colorMultiplier` 乘到 Panel/Image/Text/NineSlice/Shape 原始颜色，再应用 effectiveOpacity 和后端 AlphaMode。

因此同一帧中的画面、Clip AABB 和命中几何共享同一最终属性，不存在视觉位置已变化而交互区域仍停留在旧位置的情况。

## Hidden、Static 与 Active

动画时间由 `UIWindowRuntime::Update(deltaSeconds)` 唯一驱动，不读系统墙钟。窗口 Hidden 时 Runtime 在 Animator 前返回，不推进 elapsed、不 Build/Render/Present；重新显示后从隐藏前进度继续，且不改变用户 Pause 状态。

Running 和 Delayed 记录使 `UIFrameActivity.hasActiveAnimation=true`；只有 Paused/Queued 时为 false。完成帧因最终 Dirty 仍会 Build/Render，随后无活动记录且 Scene clean，FrameScheduler 可恢复 Static。

## 双后端与当前限制

OpenGL 与 Software layered 路径消费同一 DrawList 和仿射矩阵，Animator 本身没有后端分支。`xie_ui2d_animator_test --self-test` 在相同 delta 序列下校验两条路径的 scale、opacity、rotation 和 draw 结果。

当前未实现 Layout Size、动画组、任意关键帧时间线、贝塞尔编辑器、弹簧/阻尼、路径、Shader 参数、粒子、跨窗口动画、Theme 文件动画和 Widget 状态自动绑定。动画组没有创建占位成功接口；后续若加入，应先定义可执行的 Parallel/Sequence 生命周期与测试。
