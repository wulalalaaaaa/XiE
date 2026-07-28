# UI2D 渲染构建系统

## 范围与数据流

阶段 3D 的正式链路为：

```text
UIScene
  -> UIScene::PainterTraversal()
  -> BasicUIRenderBuilder
  -> DrawList2D
  -> OpenGL2DRenderer 或 Software2DRenderer
```

`BasicUIRenderBuilder` 只读取已布局的 Scene、解析后的 Theme 和资源查询接口。它使用逻辑坐标，不乘 DPI，不修改 Scene，不创建资源，不调用 Renderer、Presenter 或平台 API。

## Visual 到 DrawCommand 的映射

| UIVisual | DrawList2D 命令 |
| --- | --- |
| `UIPanelVisual` | `SolidRectCommand` 或 `RoundedRectCommand` |
| `UIImageVisual` | Ready 时为 `SpriteCommand`，其他资源状态为纯色占位命令 |
| `UITextVisual` | `TextCommand` |
| `UINineSliceVisual` | Ready 时为 `NineSliceCommand`，其他资源状态为纯色占位命令 |
| `UIShapeVisual::SolidRect` | `SolidRectCommand` |
| `UIShapeVisual::RoundedRect` | `RoundedRectCommand` |
| `UIShapeVisual::Line` | `LineCommand` |
| `UIShapeVisual::Circle` | `CircleCommand` |
| `UIShapeVisual::Ring` | `RingCommand` |

`std::monostate` 不输出命令。`enabled` 和 `hitTestVisible` 不影响显示；不可见、最终透明、无有效尺寸或含非有限变换的视觉会被安全跳过。构建结果通过 `UIRenderResult` 返回访问数、命令数、Clip 数、资源状态和非法数据诊断，不依赖逐帧日志。

## Painter order

Scene 持有唯一的 painter traversal。父节点先于子节点；同一父节点的子节点按 `zOrder` 从低到高，再按 `insertionOrder` 从早到晚排列。Builder 直接正序消费该结果，不复制排序逻辑；`BasicUIHitTester` 逆序消费同一结果。因此，对同时可见且可交互的节点，最后绘制者也是第一个命中候选。

子树在 traversal 中保持连续，避免全局 z 排序破坏父子和 Clip 作用域。

## Transform

布局阶段生成的 `UIComputedTransform.localToScene` 是唯一权威。每个基础命令保留本地几何和完整 `Mat3F transform`；OpenGL 执行器将矩阵应用到顶点，Software 执行器通过逆变换对真实局部几何光栅化。旋转、非均匀缩放、Pivot 和嵌套父变换不会降级成 `sceneAabb`。

渲染只要求 `localToScene` 的值有限；矩阵不可逆时仍允许生成命令。命中测试由于需要 Scene 到局部坐标转换，仍要求 `sceneToLocal` 有效。

## 颜色、Alpha 与 effectiveOpacity

Builder 直接使用布局计算的 `effectiveOpacity`，不重新遍历祖先。纯色 UI 命令统一使用 Premultiplied Alpha：颜色的 RGB 和 A 同时乘最终透明度。纹理命令保留 `TextureInfo::alphaMode`；Straight Alpha 纹理不会被误标为 Premultiplied。两种执行器都依据命令的 AlphaMode 选择等价混合规则。

`opacity=0` 不产生视觉命令，但按既有交互合同仍可命中。这是显式设计差异，不用透明度隐式实现点击穿透。

## Image Fit

- `Stretch`：填满节点本地矩形。
- `Contain`：保持宽高比，完整显示并居中。
- `Cover`：填满节点，通过裁切 UV 保持宽高比。
- `None`：使用纹理逻辑尺寸并居中；纹理大于节点时约束目标区域并同步裁切 UV。

纹理逻辑尺寸只来自 `TextureRegistry2D`。

## Text Layout 缓存

`UITextVisual` 持有上层预先创建的 `TextLayoutHandle`。Builder 只查询缓存 bitmap 的逻辑尺寸、计算对齐位置并输出 `TextCommand`，不会创建 Layout、读取字体或触发栅格化。

OpenGL Renderer 以 `TextLayoutHandle` 为键缓存 BGRA 到 RGBA 的转换结果，OpenGL Backend 以资源键缓存 GPU 纹理。每帧会重新绑定正确纹理，但相同 Layout 不会重复转换或上传。Software Renderer 直接读取同一份缓存 bitmap。

## NineSlice

Builder 将目标边框限制为非负值；当左右或上下边框总和超过目标尺寸时按比例收缩，使中心区最小为零。源边框由像素转换为归一化 UV，并限制在选定 UV 区域内。

执行器真实拆分为最多九个区域。每个区域继承同一纹理、颜色、AlphaMode 和完整 Transform，不生成负尺寸区域。

## Clip Stack

当前正式 Clip 是 Scene 逻辑坐标中的轴对齐 Rect：`ResolveSceneClipRect(node)` 返回布局保存的 `sceneRect`。规则为：先绘制节点自身；有后代且 `clipChildren=true` 时 Push；绘制完整子树；离开子树时 Pop。不可见节点和无后代节点不产生无意义 Clip，嵌套 Clip 取交集。

`BasicUIHitTester` 对裁剪祖先使用同一个辅助函数和同一个 Rect，所以渲染与命中不会出现一边裁剪、一边未裁剪。RoundedRect、Circle、Ring 和任意路径 Clip 留待具有 stencil/mask 支持的后续阶段。

OpenGL 将逻辑 Rect 按 DPI 转换为 scissor；Software Renderer 在逻辑像素光栅化时应用同一 Clip。Builder 本身不处理 DPI。

## 资源状态

- `Ready`：输出 Sprite 或 NineSlice。
- `Loading`：输出中性灰色占位。
- `Failed`：输出红色失败占位。
- 无效或陈旧 Handle：输出洋红色缺失占位。

占位视觉使用基础图形命令，不读取文件，也不携带业务含义。`TextureRegistry2D::Revision()` 和 `ITextLayoutService::Revision()` 用于触发窗口 DrawList 重建。OpenGL 图片纹理按 `TextureHandle` 缓存并绑定，不依赖全局“最后上传纹理”。

## Visual revision 与窗口缓存

`UIScene::VisualRevision()` 在 Visual、Transform 或 Children 相关失效时单调增加。`UIWindowRuntime` 保存最近成功构建使用的 Scene、Texture 和 Text revision。以下任一变化会设置 `needsBuildDrawList`：

- Visual、Layout、Transform 或结构 Dirty；
- Scene visual revision 改变；
- Theme 或逻辑窗口尺寸改变；
- Texture/Text 资源 revision 改变。

只有成功 Build 才清理对应 Render Dirty 并更新已构建 revision。Hidden 窗口不 Build；Static 且 revision 均未变化时不 Build、不 Render、不 Present。

## 验证与当前限制

`DrawListValidator` 检查 Clip 平衡、有限坐标和矩阵、正尺寸、颜色范围、纹理 Handle、圆角、圆环和线宽。自动测试覆盖所有 Visual、Fit、Transform、Painter order、Clip、资源状态、revision 和双后端命令执行；独立 `UI2DRenderTest` 使用同一 Scene 验证 GLFW/OpenGL 与 Win32 layered/software 两条路径。

当前限制：Clip 仅为变换后 Scene AABB Rect；没有任意形状 mask；文本 Layout 必须由上层预热；资源 revision 为 Registry 全局粒度；构建仍为 O(N)；当前不做跨命令批处理，以确保 painter order 不被改变。

