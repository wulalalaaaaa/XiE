# UI2D 基础布局系统

## 数据边界

布局输入由 `UITransform` 和 `UILayoutParams` 构成，输出统一写入 `UILayoutState`。布局不会覆盖用户输入，也不创建 DrawCommand、执行 HitTest、访问平台或启动动画。

```text
UIScene + logical window size
→ Measure
→ Arrange
→ UILayoutState + UIComputedTransform
→ Visual / HitTest invalidation
```

`Vec2F`、`RectF`、`InsetsF` 是唯一尺寸/矩形/边类型。没有额外的 UIRect、LayoutRect 或 FinalRect 同义类型。

## Measure

Measure 自底向上回答节点在给定可用空间中的 `desiredSize`：

1. 验证句柄、递归深度和循环状态。
2. 不可见节点返回零尺寸。
3. 从 Visual 和设计尺寸取得固有尺寸。
4. 按容器模式测量直接子节点。
5. 将自身 padding 加入子节点自然尺寸。
6. 应用 Fixed、Auto 或 Stretch。
7. 最后应用非负归一化以及 min/max。

Image 通过 `ITextureInfoProvider2D` 读取逻辑尺寸；Text 通过 `ITextLayoutService` 读取已生成布局尺寸；NineSlice 至少使用四边 border 合成的最小尺寸；Shape 使用设计 size 或 lineTo 范围。资源不可用时使用明确配置的占位尺寸，默认是零。

## Arrange

Arrange 自顶向下分配最终区域：

1. 根节点固定为 `(0,0,logicalWidth,logicalHeight)`。
2. 写入父局部空间中的 `arrangedRect`。
3. padding 只生成节点局部的 `contentRect`，不改变 arrangedRect。
4. 按节点的 LayoutMode 排列其直接子节点。
5. 根据归一化 pivot（`0..1` 表示节点宽高比例）、scale、rotation 生成 localToParent、localToScene 和 sceneToLocal。
6. 用四个视觉角点生成最终 `sceneRect` AABB。
7. 成功后清除 Layout Dirty；有变化时标记 Transform、Visual、HitTest Dirty。

`arrangedRect` 不包含旋转后的包围范围。`sceneRect` 是视觉变换后的 Scene AABB。未来精确 HitTest 先用 sceneRect 粗筛，再通过 sceneToLocal 转回节点局部空间。

## 尺寸优先级

- Fixed：使用 `UILength::value`，之后受 min/max 限制。
- Auto：使用 Visual 与子节点计算出的自然尺寸，之后受 min/max 限制。
- Stretch：Arrange 时使用父节点分配的可用尺寸，之后受 min/max 限制。

Alignment 为 Stretch 只表示在可用区域的起始对齐语义；它不会覆盖 Fixed。真正改变节点尺寸必须使用 `UISizeMode::Stretch`。

## Margin、Padding、Spacing

- Margin 属于节点外部，不进入 desiredSize 或 arrangedRect 的宽高。
- Padding 属于节点内部，只缩小 contentRect。
- Spacing 只出现在 Stack 中相邻的可见子节点之间。
- `enabled=false` 仍参与布局；`visible=false` 不测量、不占 Stack 空间，最终尺寸为零。

## 五种模式

### Absolute

子节点位置为父 content 原点加子节点 position 和 margin。Auto 容器使用所有可见子节点外部范围的包围尺寸；rotation 不参与 Measure。

### Anchor

父 content 尺寸乘以归一化后的 min/max 得到 anchor 区域，再应用 offsetMin、offsetMax 和 margin。Anchor 会限制到 `[0,1]`，min/max 颠倒时交换。单点 anchor 使用节点期望尺寸；跨区 anchor 在对应 SizeMode 为 Stretch 时填满区域。

### Overlay

所有可见子节点共享父 content 区域，按双轴 alignment、margin 和 SizeMode 排列。Auto 容器取子节点最大外部尺寸。

### HorizontalStack

可见子节点从左到右排列。固定占用、margin 和 spacing 先扣除；主轴为 Stretch 的子节点等分剩余宽度。交叉轴使用 height SizeMode 与 verticalAlignment。

### VerticalStack

规则与 HorizontalStack 对称；主轴 Stretch 子节点等分剩余高度，交叉轴使用 horizontalAlignment。

## 根节点、DPI 与 Dirty

Layout 永远使用 `UIWindowRuntime` 提供的逻辑窗口尺寸。DPI 变化会触发重新布局，以便字体或资源逻辑度量改变，但 BasicLayoutEngine 不把逻辑尺寸乘以 DPI；framebuffer 像素换算仍属于 Renderer Backend。

创建、销毁、Reparent、Layout/Transform/Visual 变化、可见性变化、窗口尺寸和 DPI 变化都会设置 Layout Dirty。当前任何 Layout Dirty 都执行一次 Scene 级 O(N) Measure/Arrange；无 Dirty 时立即返回且不遍历 Scene。遍历复用 scratch 状态数组，并有 512 层深度保护。

## 调试与限制

`UILayoutDebugStats` 记录 total/visible/measured/arranged/maxDepth。`BasicLayoutEngine::DumpScene` 仅在显式调用时生成层级矩形文本，不在帧循环输出日志。

当前不实现 Grid、Flex、Wrap、Scroll、虚拟化、百分比约束、旋转参与 Measure 或子树增量布局。Stack Stretch 不支持权重，只做首次等分，min/max 截断后不二次分配余量；Anchor Auto 不求解百分比约束产生的循环依赖；资源逻辑尺寸变化必须由资源协调层标记 Dirty。当前纹理注册表没有独立 logical-size 元数据，因此纹理像素尺寸按 1:1 逻辑单位解释。
