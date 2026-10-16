# Crystal Phase 3 Delivery Report

日期：2026-08-11  
范围：Crystal Phase 3「自由项目工作空间」；保留 Phase 1、Phase 2、Phase 2.5 能力，不进入 Phase 4。

## 1. Phase 3 总体架构

Crystal 继续采用「XiE 客户端 + Crystal Server」结构。服务端负责项目事实、SQLite 持久化、REST、WebSocket、Activity 与并发锁；客户端由 `CrystalWorkspace` 统一持有 Project / Space / Entity / View / Board / Layer / Ink / Query / Widget，并通过主线程事件入口应用实时事件。

空间组织模型为：

`Project → Spaces → User Layers → Entity Views + Boards + Ink`

System/HUD、Semantic Graph、Ink/Drawing 是显示维度；User Layer 是用户组织维度，二者没有混用。

## 2. Schema 系统

新增 Project 级 `entity_schemas`，包含 `id`、`project_id`、`name`、`display_name`、`icon`、`description`、时间戳，并由 `property_definitions` 描述字段。

Schema-defined Entity 使用 `entities.schema_id` 关联 Schema；服务端根据 Schema 校验属性，C++ 使用通用 `CrystalEntity`，不为每种用户类型增加 C++ struct 或独立表。

## 3. Property 系统

支持 `Text`、`MultilineText`、`Number`、`Bool`、`Enum`、`Date`、`EntityReference`、`EntityReferenceList`、`Tags`、`Progress`、`URL`。

Property Definition 包含名称、显示名、类型、默认值、必填标志与 Enum options；服务端支持默认值、类型、范围、枚举项、引用格式和未知字段校验。未实现 Formula、Script、Computed Expression、Rich Text、Embedded Database。

## 4. Built-in / Custom Entity 共存方案

Phase 2 的 Task、Worker、System、Milestone、Scene 继续保留 typed detail table 和既有业务逻辑；Bug、Decision 以及用户新建类型使用 `schema_id + entity_property_values`。

两者都可以进入 Space、创建多个 View、建立 Relationship、参与查询、加入 User Layer、显示 Inspector，并可被 Ink Anchor。Relationship 只按 Entity ID 工作，不区分内置或自定义类型。

## 5. Generic Inspector

客户端通过 `CrystalEntity.schema` 与 `CrystalEntity.properties` 读取字段，使用统一属性行渲染；Text、MultilineText、Date、URL 支持文本编辑，Bool / Enum 支持循环修改，其他属性类型以通用值展示并由服务端负责完整校验。

内置 Task 等仍保留 Phase 2 专用 Inspector 字段，避免为统一形式破坏既有流程；Custom Entity 不存在 BugInspector 或 DecisionInspector。

## 6. View Configuration

View 类型支持 `Node`、`CompactNode`、`Card`、`StatusCard`、`Badge`。同一个 Entity 可以在不同 Space 创建不同 View，Entity 本体不变。

`entity_views.visible_properties_json` 保存 View 自己的 `visible_properties[]`，因此隐藏属性不会回写 Entity。服务端与 C++ 模型均支持 View 配置、可见性、层、尺寸、排序和 Board 局部坐标。

## 7. Board

新增属于 Space 的 Board 视觉对象，包含 Position、Size、Title、Background、Border、Lock、Layer、Z-order。Board 不是 Entity，也不是 Kanban 的特殊别名。

已实现 Board CRUD、Attach/Detach View、Board 移动时子 View 的世界坐标同步；删除 Board 不删除其中 Entity，默认保留 View 并清除 Board 关联。

## 8. User Layer

实现 User Layer Create、Rename、Delete、Visibility、Lock、Reorder。Layer 可承载 Entity View、Board、Ink 和 Dashboard Widget。

删除 Layer 时将视觉对象迁移到 Main，Entity 本体不删除；客户端渲染同时检查 Layer 可见性，隐藏 Layer 后视觉对象消失，重新显示可恢复。

## 9. Drawing / Ink 新能力

`InkStroke` 增加 Pen Width、Basic Pen Color、Highlighter，以及 `Freehand`、`StraightLine`、`Arrow`、`Rectangle`、`Ellipse` 五种形状。客户端 Renderer 已分别绘制这些形状，并对选中笔画显示高亮。

原有擦除与向量点序列保持兼容；Graph / Draw / All 显示模式会改变 Ink 与 Graph 的显示优先级。

## 10. Ink Group / Anchor

Ink 支持 Select Stroke、删除、Group；服务端以 `group_id` 保存 Ink Group，客户端可通过统一 Workspace API 应用分组返回的笔画。

Anchor 支持 Entity View 与 Board，保存 `anchor_entity_id` / `anchor_board_id` 与局部偏移。Board 移动时，已锚定 Ink 与 Board 内容一起更新；删除 Board 不删除 Ink。

## 11. Space 管理

Space 支持 Create、Rename、Duplicate、Delete、Reorder，并保存 Camera、Zoom、View Type、Boards、Views、Ink、Layers、Widgets。删除 Space 只删除该 Space 的布局数据，不删除 Entity。

客户端 HUD 已显示当前 Space、最近 Space 和 Space 切换入口；Workspace 复用现有缓存模型，不重新创建所有业务对象。

## 12. Query View

新增 `query_views`，保存 project、space、entity schema、filters、sort、limit。过滤器支持 `Equals`、`NotEquals`、`Contains`，多个过滤器按 AND 组合。

服务端 Workspace 返回 Query 结果；客户端在 Entity Created / Updated / Deleted 和实时事件应用后刷新本地 Query 结果，支持自定义属性及内置字段查询。

## 13. Dashboard

Dashboard 不是固定页面，而是 Space 内的 `dashboard_widgets`。第一版支持 EntityList、SceneLockList、WorkerList、Progress、Counter、Text、RecentActivity 的基础数据模型与 EntityList / SceneLockList / WorkerList / RecentActivity 渲染。

Widget 可配置位置、尺寸、Layer、可见性和 Query View 绑定；服务端返回实时事实，不复制成独立 Entity。

## 14. Activity

新增服务端 `activities` 事实日志与 `activity.created` 实时事件。记录 Entity 创建/状态更新、Relationship 创建或可见性更新、Scene Lock Acquire/Release、Ink Group 等高价值操作。

Dashboard 的 RecentActivity 直接读取服务端 Activity；没有记录每次拖拽、Camera 移动或每一笔 Ink，避免噪音。

## 15. Decision Schema

Decision 作为官方示范 Schema Seed 到 XiE 与 P_HighSchool，使用通用 Schema/Property 机制，不增加独立 C++ Entity 类型或业务表。

默认字段包括 Status、Context、Decision、Reason、Date、RelatedSystem；P_HighSchool 中创建了 `Story Segment 作为单次执行单位` 示例实体，并配置了多种 View。

## 16. Bug Schema

Bug 作为官方示范 Schema Seed 到 XiE 与 P_HighSchool，默认字段包括 Status、Severity、Priority、Owner、RelatedSystem、Description。

P_HighSchool 中创建了 `NPC无法通过斑马线` 示例 Bug，使用通用 `schema_id + properties`，并在 Project Overview 与 Architecture 以不同 View Type 展示。

## 17. Project Template

提供 Blank、Software、Game Development 三个服务端模板。模板只包含初始 Schemas、Spaces、Boards、View/Widget 配置，不复制实际 Task、Worker 在线状态、Scene Lock 或 Activity 历史。

Game Development 默认提供 Overview、Architecture、Current Sprint，以及 Bug、Decision Schema 和基础 Dashboard 配置；Apply 后仍可由用户继续删除或修改。

同时实现 Save Current Project As Template，保存当前项目的 Schema、Space、Layer、Board、View Configuration、Query Widget。

## 18. Display Modes

HUD 增加 All、Graph、Draw、Presentation 四种显示模式，模式是显示/输入状态，不创建额外 Space。

All 显示 Graph 与 Ink；Graph 隐藏 Ink；Draw 隐藏 Graph/Board/Entity View，突出手绘；Presentation 隐藏 Inspector 与编辑提示。模式切换已接入客户端 Workspace 和 Renderer。

## 19. Database V3 Migration

数据库 Schema Version 从 2 升为 3，采用 additive migration。新增或补齐 `entity_schemas`、`property_definitions`、`entity_property_values`、`boards`、`query_views`、`dashboard_widgets`、`activities`、`project_templates`，并为旧表追加 Schema、Board、Layer、Ink Shape/Anchor、Relationship Visibility 等字段。

Migration 保留 Projects、Spaces、Entities、Views、Relationships、Ink、Locks 以及 Phase 2 detail tables；没有删库重建。V2 fixture → V3 的实体、关系和锁相关旧数据已验证保留。

## 20. API

新增或扩展资源化 REST API：

- Schema / Property Definition CRUD；
- Entity custom create / patch / delete；
- Space CRUD / duplicate / reorder；
- Board CRUD、View Attach/Detach；
- Layer CRUD、visibility、lock、reorder；
- Query View CRUD；
- Dashboard Widget CRUD；
- Ink Stroke CRUD、Ink Group、Entity/Board Anchor；
- Relationship visibility/detail 更新；
- Recent Activity 查询；
- Project Template list/apply/save。

原有 Phase 2 Entity、View、Relationship、Scene Lock API 保持兼容。

## 21. Realtime Events

Schema、Board、Layer、Space、Query View、Widget、Activity、Relationship、Ink、Entity、Scene Lock 等事件都经由既有 Network Thread → thread-safe queue → Main Thread → `CrystalWorkspace::ApplyRealtimeEvent` 路径应用。

客户端没有让 WebSocket 线程直接修改 Workspace；Board 更新会同步子 View 和锚定 Ink，Entity 更新会刷新 Query 结果，Relationship visibility 更新会同步 Graph Edge。

## 22. XiE Engine 新增通用能力

本阶段进入 XiE Engine 的是通用能力：Stroke shape/tool model、Ink selection state、通用 ToolController 选中 Stroke、HTTP DELETE body overload，以及客户端可复用的选择/绘制基础。

Crystal 专属的 Bug、Decision、Dashboard、Scene Lock、Query、Template、Project Schema 仍留在 `XiE_Projects/Crystal`，没有把业务规则下沉进 Engine。

## 23. Crystal 专有能力

Crystal 层新增 Schema/Property/Custom Entity 模型与 API Client、Board/Layer/Query/Widget/Activity Workspace 聚合、Display Mode、通用 Custom Inspector、Relationship Visibility、Board/Entity Ink Anchor，以及 Phase 3 Renderer 与 HUD 展示。

客户端仍通过统一 Workspace 更新入口处理 REST 结果和实时事件，保留 Phase 2 的 typed details 和 Scene Lock 流程。

## 24. P_HighSchool 实际配置

P_HighSchool 已创建 Project Overview、Architecture、NPC Architecture 等 Space，并在 Project Overview 配置 Active Tasks、Open Bugs、Recent Activity、Workers、Scene Locks Widget；Overview 面板显示 Current Milestone、Active Tasks、Workers、Scene Locks、Systems。

Architecture 中已创建 Gameplay、Narrative、World Systems 三个 Board，并放置 Task System、Story System、Weather System、Survival Stats、NPC System；同时存在真实 Relationship Edge、Decision、Bug 和至少一条 Ink 架构注释数据。

## 25. C++ Tests

验证结果：

`Custom Entity Without New C++ Type: PASS`

`Multi View Custom Entity: PASS`

`Board Child Transform: PASS`

`User Layer Visibility: PASS`

`Ink Group + Board Anchor: PASS`

`Query Dashboard Live Update: PASS`

核心命令 `cmake --build vs_build --config Debug --target crystal_core_tests --parallel 1` 通过；直接运行 `crystal_core_tests.exe` 输出 `Crystal core checks=33 status=PASS`。Crystal 定向 CTest 为 1/1 PASS，`crystal_client` Debug 构建通过。

全仓 CTest 本次为 39 项 PASS、7 项 Not Run；7 项均为既有 AudioRuntime / VoiceMixer 缺失可执行文件，不是 Crystal 测试失败。

## 26. Server Tests

命令：`CrystalServer/.venv/Scripts/python.exe -m pytest -q tests`

结果：`19 passed`。覆盖 V3 seed、Property 默认值与校验、V2→V3 migration、Schema/Custom Entity CRUD、所有 View Type、Board 子 View 移动、Layer visibility/rename、Query 与 Dashboard、Ink Shape/Group/Board Anchor、Relationship visibility、Space duplicate/delete、Template apply/save、Activity，以及既有 Phase 2/2.5 API/WebSocket/Revision/Lock 测试。

## 27. Phase 2.5 Verification Debt 当前状态

FastAPI 依赖已在项目 `.venv` 恢复，原先的 FastAPI Integration、Revision Conflict HTTP E2E、Two Client Realtime Sync、Scene Lock 并发等验证已随全量服务端测试通过。

`Scene Lock Concurrent Acquire 100 Runs: PASS`

真实网络断线后再连接的外部网络重连场景没有在本次本地测试中制造并验证，状态为 `NOT TESTED`；客户端仍保留 Phase 2.5 authoritative reload 与连接状态处理。

## 28. 修改文件列表

本阶段相关文件如下；工作区中原有的其它未提交改动未清理、未重置：

- Server：`XiE_Projects/Crystal/CrystalServer/app/database.py`、`app/entities.py`、`app/main.py`；`tests/test_phase2_model.py`、`tests/test_phase3_model.py`、`tests/test_phase3_server.py`。
- Crystal Model / Network：`XiE_Projects/Crystal/Source/Model/CrystalModels.h/.cpp`、`Source/Net/CrystalApiClient.h/.cpp`、`Source/Net/CrystalRealtimeEvent.h/.cpp`。
- Crystal Workspace / UI：`XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.h/.cpp`、`Source/UI/CrystalRenderer.cpp`、`Source/Main.cpp`、`tests/CrystalCoreTests.cpp`。
- XiE Engine 通用部分：`Engine/UI2D/InkStroke.h`、`Engine/UI2D/SelectionModel.h/.cpp`、`Engine/UI2D/ToolController.h/.cpp`、`Engine/Network/HttpClient.h/.cpp`。

## 29. 未完成内容

- Generic Inspector 的 Number、EntityReference/List、Tags、Progress 目前完成通用显示与服务端校验，尚未完成专用的数值控件、引用选择器和标签编辑器。
- Ink 的单 Stroke 选中、形状、分组和 Anchor 数据链路已完成；多 Stroke 框选后的完整拖拽变换、Duplicate UI 仍可继续完善。
- 真实断线/重连网络环境和人工逐屏视觉验收未在本轮自动化验证中完成。
- 全仓 CTest 的 7 个缺失 AudioRuntime / VoiceMixer 可执行文件属于工作区其它项目，未在 Phase 3 范围内补建。

## 30. Phase 4 建议

Phase 4 可优先补齐专用 Property Editor、Ink Group Transform、Layer Panel 交互、Board/Widget 拖拽编辑、Query 编辑器和真实 Reconnect E2E；随后再做主题皮肤、视觉规范、动画与更完整的手动 UX 验收。Phase 3 至此停止，不进入 Git、CI 或其它阶段。
