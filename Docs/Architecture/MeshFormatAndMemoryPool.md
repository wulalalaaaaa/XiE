# XiE Mesh 格式与内存池说明

## 1. 目标

本轮的目标是三件事：

1. 定义一个简约、可控的引擎自有网格格式（`.xmesh`）。
2. 保持对主流交换格式的读取能力（当前支持 `.obj`）。
3. 引入引擎自管内存池，承载网格数据内存分配。

关键入口：[`Engine/Renderer/MeshAsset.cpp:325`](../../Engine/Renderer/MeshAsset.cpp#L325)

---

## 2. XiE 自有格式（`.xmesh`）

### 2.1 格式规则（最小语法）

必须包含头：

```text
xie_mesh 1
```

支持行：

- `v x y`：2D 顶点
- `f i j k`：三角面（**0-based** 索引）
- `f i j k l`：四边面（加载时自动三角化）
- `# ...`：注释

解析实现：
- 头校验：[`Engine/Renderer/MeshAsset.cpp:147`](../../Engine/Renderer/MeshAsset.cpp#L147)
- 顶点行：[`Engine/Renderer/MeshAsset.cpp:155`](../../Engine/Renderer/MeshAsset.cpp#L155)
- 面行：[`Engine/Renderer/MeshAsset.cpp:167`](../../Engine/Renderer/MeshAsset.cpp#L167)

### 2.2 示例文件

当前项目默认示例：
- [`XiE_Projects/test2Dgame/Assets/Mesh/main.xmesh`](../../XiE_Projects/test2Dgame/Assets/Mesh/main.xmesh)

默认加载路径：
- [`Engine/Core/Application.cpp:28`](../../Engine/Core/Application.cpp#L28)

---

## 3. 主流格式支持（当前）

### 3.1 OBJ 支持

已支持：

- `v x y [z]` 顶点（读取 `x/y`，可忽略 `z`）
- `f a b c` 三角面
- `f a b c d` 四边面
- `f a/b/c` 这类 slash token（会取第一个索引）

实现位置：
- OBJ 解析函数：[`Engine/Renderer/MeshAsset.cpp:194`](../../Engine/Renderer/MeshAsset.cpp#L194)
- face token 解析：[`Engine/Renderer/MeshAsset.cpp:65`](../../Engine/Renderer/MeshAsset.cpp#L65)

### 3.2 Legacy 兼容

仍兼容旧 `v0:/v1:/v2:` 三角文件，便于平滑迁移。  
实现：[`Engine/Renderer/MeshAsset.cpp:240`](../../Engine/Renderer/MeshAsset.cpp#L240)

---

## 4. 三角形与四边形策略

### 4.1 当前策略

- 输入可为三角形或四边形。
- GPU 提交统一为三角形索引（`f quad -> 2 triangles`）。

三角化位置：
- [`Engine/Renderer/MeshAsset.cpp:100`](../../Engine/Renderer/MeshAsset.cpp#L100)

### 4.2 为什么这样做

- 硬件栅格化天然以三角形为核心图元。
- 四边面如果不共面会有二义性，最终仍要确定三角划分。
- 在加载阶段三角化，运行期开销更稳定。

---

## 5. 内存池设计（网格数据）

### 5.1 FixedBlockMemoryPool

提供固定块池 + 超限回退分配：

- 声明：[`Engine/Utils/MemoryPool.h:8`](../../Engine/Utils/MemoryPool.h#L8)
- 分配：[`Engine/Utils/MemoryPool.cpp:43`](../../Engine/Utils/MemoryPool.cpp#L43)
- 释放：[`Engine/Utils/MemoryPool.cpp:64`](../../Engine/Utils/MemoryPool.cpp#L64)
- 统计：[`Engine/Utils/MemoryPool.cpp:86`](../../Engine/Utils/MemoryPool.cpp#L86)

默认网格池实例：
- [`Engine/Utils/MemoryPool.cpp:109`](../../Engine/Utils/MemoryPool.cpp#L109)

### 5.2 MeshAsset 如何使用

网格解析完成后，不再长期依赖 `std::vector` 存储，而是拷贝到池分配内存：

- 池化写入：[`Engine/Renderer/MeshAsset.cpp:390`](../../Engine/Renderer/MeshAsset.cpp#L390)
- 池化释放：[`Engine/Renderer/MeshAsset.cpp:428`](../../Engine/Renderer/MeshAsset.cpp#L428)

这样做的好处：

- 引擎可掌控网格内存生命周期与统计信息。
- 为后续引入更细粒度资源池（纹理池、材质池）打样。

---

## 6. 渲染链路更新点

- 后端接口从 triangle-only 升级为 mesh + indices：
  - [`Engine/Renderer/IRenderBackend.h:13`](../../Engine/Renderer/IRenderBackend.h#L13)
- OpenGL 使用 EBO + `glDrawElements`：
  - [`Engine/Renderer/OpenGLBackend.cpp:101`](../../Engine/Renderer/OpenGLBackend.cpp#L101)
  - [`Engine/Renderer/OpenGLBackend.cpp:157`](../../Engine/Renderer/OpenGLBackend.cpp#L157)
- 2D Feature 上传整网格：
  - [`Engine/Renderer/Feature/Renderer2DFeature.cpp:84`](../../Engine/Renderer/Feature/Renderer2DFeature.cpp#L84)

---

## 7. 后续可扩展方向（建议）

1. `.xmesh` 增加可选法线/UV（仍保持核心简约）。
2. OBJ 支持负索引和多组对象（`o/g`）拆分。
3. 内存池增加按类型分类统计（Mesh/Texture/Material）。
4. 增加离线转换器：`obj -> xmesh`，将运行时解析成本前移。

