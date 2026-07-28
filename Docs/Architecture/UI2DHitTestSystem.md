# UI2D 命中测试系统

## 范围

阶段 3C 只回答一个问题：给定窗口逻辑坐标，返回视觉 painter order 中最上层的可命中节点，以及稳定的根到目标路径。它不派发 Pointer 事件，不维护 Hover、Capture、Focus，也不读取 Renderer、平台窗口或物理 framebuffer。

## 坐标空间

`BasicUIHitTester` 的输入是逻辑像素 Scene Space。DPI 只由窗口和布局阶段处理一次，HitTest 不再次应用 DPI，也不重算 Anchor、Margin、Stack 或父子位置。

布局为每个节点保存：

- `arrangedRect`、`contentRect`；
- `localToScene`、`sceneToLocal` 和 `inverseValid`；
- 变换后的 `sceneRect` AABB；
- `effectiveVisible`、`effectiveEnabled`。

节点局部命中矩形固定为 `(0, 0, arrangedRect.width, arrangedRect.height)`。输入点通过布局预先计算的 `sceneToLocal` 转换；旋转、缩放、Pivot 和嵌套父变换因此只有一个权威来源。矩阵不可逆时节点安全拒绝，HitTest 不临时求逆。

## HitShape 与边界规则

`UIHitShape` 支持 `None`、`Rect`、`RoundedRect`、`Circle` 和 `Ring`：

- `None` 永远拒绝；
- `Rect` 使用局部矩形；
- `RoundedRect` 将半径限制到 `0..min(width,height)/2`；
- `Circle` 以局部矩形中心为圆心，未指定半径时使用短边的一半，显式半径限制在局部矩形内；
- `Ring` 使用包含边界的 `innerRadius <= distance <= outerRadius`。两者均未指定时，内外半径为默认外半径的 50% 和 100%；非法顺序归一化到有效范围。

所有形状包含边界。纯函数位于 `UIHitGeometry`，不依赖 Scene、平台或 Renderer；NaN、无限值、零/负尺寸安全返回 false。

## 正式命中流程

`BasicUIHitTester::HitTest` 按以下顺序执行：

1. 验证 Scene Position、限定根和布局是否有效；
2. 取得 Scene 持有的共享 `UITraversalEntry` painter traversal，并限定到指定子树；
3. 从最上层向下逆序扫描；
4. 过滤 visible、enabled、hitTestVisible、HitShape、尺寸和矩阵状态；
5. 用 `sceneRect` 做 Scene Space AABB 粗筛；
6. 验证所有启用 `clipChildren` 的祖先；
7. 用候选节点 `sceneToLocal` 转换并执行精确 HitShape 判断；
8. 第一个通过的节点立即返回，并构造 route。

HitTest 不复制节点数组，也不为每个候选分配临时容器。当前扫描复杂度为 O(N)，祖先裁剪验证最坏为 O(depth)，命中后 route 构造为 O(depth)。`UIHitTestContext` 保留限定根能力，后续空间索引可以替换候选来源而不改变精确判断和结果合同。

## Clip 链

`clipChildren=true` 只裁剪该节点的后代，不裁剪节点自身。阶段 3D 将正式共享合同固定为 Scene 逻辑坐标中的轴对齐 Rect：HitTest 与 RenderBuilder 都调用 `ResolveSceneClipRect`，直接复用布局保存的 `sceneRect`。嵌套 Clip 必须全部通过。

这一限制与当前 DrawList `PushClipRect`、OpenGL scissor 和 Software clip 的能力一致，确保画面与命中使用相同祖先范围。候选节点自身仍使用 `sceneToLocal` 和精确 `UIHitShape`；RoundedRect、Circle、Ring 和任意矢量路径 Clip 尚未作为裁剪形状实现。

## 可交互状态

默认节点必须同时满足：句柄有效、`visible && effectiveVisible`、`enabled && effectiveEnabled`、`hitTestVisible`、HitShape 非 None、有效正尺寸、Arrange 有效且 Scene Transform 可逆。

父节点不可见会让子树不可见；父节点 disabled 会通过 `effectiveEnabled` 禁用整个子树。`UIHitTestContext::includeDisabled` 是显式调试/特殊查询开关。`opacity=0` 不影响命中，不通过浮点透明度隐式实现穿透。

## Painter order

Scene 是 painter order 的唯一来源。`UITraversalEntry` 采用父节点先于子节点的 preorder；同一父节点下，兄弟按 `zOrder` 从低到高、再按全 Scene 单调 `insertionOrder` 从早到晚排序。因此：

- 子节点默认画在父节点之上；
- 更高 `zOrder` 后绘制；
- 相同 `zOrder` 时后插入节点后绘制；
- HitTest 逆序扫描，选择最后绘制的候选。

子树保持连续，不做破坏 Clip/父子语义的全局 zOrder 扁平重排。`BasicUIRenderBuilder` 直接消费 `UIScene::PainterTraversal()`，不另建排序规则。

## 结果与 route

`UIHitTestResult` 包含 `hit`、`target`、`scenePosition`、`targetLocalPosition` 和 `route`。命中时 route 顺序为查询根 `Root -> ... -> Parent -> Target`，目标一定是最后一项。限定子树查询时，context root 是 route 的第一项。未命中时 target 无效且 route 为空。

`targetLocalPosition` 允许下一阶段 InputRouter 避免对目标重复坐标转换；事件路由本阶段尚未实现。

## Dirty、revision 与生命周期

创建、销毁、Reparent、Layout、Transform、visible、enabled、hitTestVisible、HitShape、zOrder 和 clipChildren 的正式 Scene setter 都会使 HitTest Dirty；布局最终状态变化也会失效 HitTest。Scene 保存单调 `HitTestRevision()`，当前采用 Scene 级失效，不构建无价值的空间缓存。

结构和排序变化会重建 Scene 持有的 painter traversal。销毁使用带 generation 的句柄，旧句柄不会残留在 traversal 或 route。HitTester 是只读服务，不清理 Dirty，不修改节点。

`UIWindowRuntime::HitTest` 在窗口隐藏或 Layout Dirty 时明确返回未命中，不隐式执行布局，确保 Runtime 的 Input/Mutation/Layout/Animation/Render 更新顺序不被查询打乱。

## 调试与未来扩展

`UIHitTestStats` 可选记录 visited、AABB reject、Clip reject、Geometry reject 和不可逆矩阵 reject。显式启用 `debugDumpEnabled` 后，`BasicUIHitTester::LastDebugDump()` 提供候选过程；默认不打印日志。

后续空间索引可按 `sceneRect` 和 `HitTestRevision` 生成候选，但必须继续使用共享 painter order、精确 `sceneToLocal`、HitShape 和 Clip 链。阶段 3D 的 `BasicUIRenderBuilder` 已消费同一 traversal 和 Rect Clip 规则；下一阶段 InputRouter 可直接使用 target、route 和 targetLocalPosition 实现事件派发。
