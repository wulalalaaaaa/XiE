# Crystal Phase 2 交付报告

日期：2026-08-10

范围：在 Phase 1 XiE 2D 能力之上，完成 Crystal 的 Entity Graph、多 Space / Multi View、Task / System / Worker / Milestone / Scene、语义关系、Revision、增量实时事件、Inspector、基础 Kanban、搜索和 Anchored Ink MVP。

## 1. Phase 2 总体完成情况

Phase 2 的数据模型、服务端 API、C++ 客户端同步路径和 MVP UI 已落地。P_HighSchool 的项目数据由服务端 seed 创建，不硬编码在客户端。

四项必须单独声明的结果如下：

- `Multi View Same Entity: PASS` —— 服务端数据库 fixture 和 C++ workspace 序列化测试均确认一个 Task 对应多个 Space View，Entity 数量不复制。
- `Revision Conflict Protection: NOT TESTED` —— 服务端实现了 `expected_revision` / 409 Conflict 路径，集成测试用例已写入，但当前环境缺少 FastAPI，未执行 HTTP 端到端验证。
- `Scene Lock Concurrent Acquire 100 Runs: PASS` —— 100 个并发竞争中严格 1 个成功、99 个被拒绝。
- `Two Client Realtime Sync: NOT TESTED` —— 双客户端 WebSocket 集成测试已写入，但当前环境缺少 FastAPI，未执行双客户端端到端验证。

## 2. Phase 1 是否保持兼容

保持以下兼容能力：

- Phase 1 的 Text、Note、Scene、Ink Stroke、Scene Lock 数据仍可读取。
- 增加了 `schema_version` 和从 Phase 1 DB 到 Phase 2 DB 的 additive migration，不依赖删除数据库重建。
- Phase 1 Scene 的 `data_json.description`、Lock 的 `owner_id/task` 会迁移到 Phase 2 字段。
- Scene Lock 的 `BEGIN IMMEDIATE` 原子抢锁路径保留，100 次竞争复验通过。
- C++ 客户端保留 `workspace.state` 解析兼容，但 Phase 2 服务端不再通过 WebSocket 推送完整 workspace；初次连接和重连由客户端使用 REST authoritative snapshot。
- 未修改无关 XiE Core / Renderer 冲突文件。

## 3. Entity 数据模型

`CrystalEntity` 统一承载 `id/project_id/entity_type/title/description/revision/data/details`。类型包括 Phase 1 的 `text/note/scene` 和 Phase 2 的 `task/system/worker/milestone`。

服务端采用 normalized detail tables：

- `task_details`
- `system_details`
- `worker_details`
- `milestone_details`
- `scene_details`

`data_json` 只保留 Phase 1 兼容和通用扩展用途，业务状态不再全部依赖一个无结构 JSON。

## 4. Entity / View 分离实现

`entity_views` 独立保存 `view_id/entity_id/space_id/x/y/width/height/view_type/layer_id/visible/revision`，通过 `(entity_id, space_id)` 唯一约束保证同一 Entity 在同一 Space 只有一个 View。

删除 View 只删除 `entity_views` 行，不删除 Entity；删除 Entity 才级联删除它的 View 和关系。

## 5. Multi Space / Multi View 实现

支持 `Graph` 和 `TaskBoard` 两种 Space 类型，以及 `Node` / `CompactNode` 两种 View 类型。P_HighSchool seed 中的 `task-npc-scene` 同时存在：

- Overview：标准 Node
- Current Sprint：CompactNode
- NPC Architecture：标准 Node

C++ 的 `V` 快捷操作可将当前选中 Entity 添加到当前 Space；新建 View 使用 revision `0`，由服务端创建为 revision `1`。

`Multi View Same Entity: PASS` 的依据：

- `tests/test_phase2_model.py::test_phase2_seed_and_multi_view_fixture`
- `Tests/CrystalCoreTests.cpp` 的 workspace / view serialization 检查

## 6. Relationship 实现

支持且仅支持以下七种类型：

`depends_on`、`blocks`、`belongs_to_system`、`assigned_to`、`belongs_to_milestone`、`integrates_into`、`related_to`。

服务端校验：

- Source / Target 必须存在。
- 两个 Entity 必须属于同一 Project。
- 禁止自连接。
- 同方向同类型重复关系通过唯一约束拒绝。
- `assigned_to`、`belongs_to_system`、`belongs_to_milestone` 会同步维护 Task 的权威 typed reference。

Edge 显示关系标签、方向箭头和差异化颜色；选中 Edge 后可按 Delete 删除。Connect Tool 的流程为点击 Source、点击 Target，默认 `related_to`；Source 已选中时按 `C` 循环选择七种关系类型，再点击 Target 创建关系。

## 7. Revision / Conflict 实现

Entity 和 View 均有 revision。Entity PATCH 可提交 `expected_revision`：

- 匹配：更新成功，revision 加一。
- 过期：返回 HTTP 409，包含 `revision_conflict`、期望 revision 和当前 Entity。

C++ 的 Entity 更新、标题编辑、Task 状态修改、Inspector 编辑和 View 保存都携带当前 revision。Ink 沿用 Phase 1 策略，不加入 CRDT。

`Revision Conflict Protection: NOT TESTED`：实现和测试用例已完成，但因 FastAPI 依赖不可用未执行 TestClient HTTP 验证。

## 8. Task 实现

Task typed fields 包括 Title、Description、Status、Priority、Assigned Worker、Milestone、System、Branch、Allowed Paths、Forbidden Paths、Acceptance Criteria、Started At、Completed At。

Status 固定为：`Backlog`、`Ready`、`InProgress`、`Review`、`Testing`、`Done`、`Blocked`；Priority 固定为：`Low`、`Normal`、`High`、`Critical`。

Graph Node 和 Task Card 显示 Title、Status、Owner、Priority；其余字段在 Inspector 查看和编辑。

## 9. Worker 实现

Worker typed fields 包括 WorkerId、WorkerType、Status、Branch、LastActivityAt。`CurrentTask` 和 `CurrentLocks` 均由服务端查询计算，不是第二份可独立写入的事实。

Worker 节点和 Inspector 显示名称、类型、状态、当前任务、当前 Lock 数量及 Branch 相关信息；Lock 增量事件到达后客户端会重算 Worker 的当前 Lock 摘要。

## 10. System 实现

System typed fields 包括 Status、Owner、Progress。状态固定为 `Planned`、`Developing`、`Testing`、`Stable`、`Deprecated`。

P_HighSchool 已 seed：Task System、Story System、Weather System、Survival Stats、NPC System。System 使用普通 Node 和关系 Edge 表达，不实现复杂嵌套节点布局。

## 11. Milestone 实现

Milestone typed fields 包括 Status、Target Date、Progress、Acceptance Criteria。P_HighSchool 已 seed `Alpha Gameplay Integration`。

Progress 按关系查询自动计算：`Done Task Count / Total Related Task Count`，同时返回 `done_task_count` 和 `total_related_task_count`。

## 12. Scene Entity 与 Lock 分离方式

Scene Entity 只保存 Scene Name、Asset Path、Description、Project；Scene Lock 仍由 `scene_locks` / `SceneLockService` 保存当前占用状态。

Scene Node 在 UI 中组合显示 Entity 与实时 Lock 状态、Owner、Task。删除 Scene View 不会删除 Entity，也不会释放 Lock；删除仍有效或 stale 的 Scene Lock 会拒绝删除 Scene Entity。Acquire 支持 `worker_id`、`task_id`、`task_note`、Branch，并校验 Task 和 Scene 属于同一 Project。

## 13. Inspector

Inspector 是固定在右侧的 HUD / System Layer，不参与 Infinite Canvas 的世界坐标和缩放。

- Task：Title、Description、Status、Priority、Owner、Branch、Acceptance Criteria。
- System：Title、Description、Status、Progress。
- Worker：以查看为主，展示 Current Task / Current Locks。
- Milestone：Title、Description、Status、Target Date、Progress。
- Scene：Title、Description、Asset Path、Lock 状态。

点击 Inspector 字段后输入文字会走 revision-aware 更新；Status、Priority、Owner、System Status、Milestone Status 等离散字段点击循环选择。冲突响应保留服务端当前版本信息。

## 14. Connect Tool

Connect 已从 Phase 1 预留状态接入 Crystal 业务层，Engine 只负责通用命中和输入路由。Crystal 负责关系类型、Project 校验、服务端创建、当前客户端增量更新和删除。

当前 MVP 的轻量 Relationship Type 选择为 `C` 键循环，不引入复杂弹窗或独立业务 Widget。

## 15. Kanban

`Current Sprint` 是 `TaskBoard` Space，不是第二个任务数据库。七列直接映射 Task.status：Backlog、Ready、InProgress、Review、Testing、Done、Blocked。

拖动 Task Card 到列时调用 Task Entity PATCH，Graph Node、Inspector、其他已连接客户端和 Kanban 共用同一个 Task.status。

## 16. WebSocket 新事件

服务端发送增量事件：

- `entity.created`
- `entity.updated`
- `entity.deleted`
- `view.created`
- `view.updated`
- `view.deleted`
- `relationship.created`
- `relationship.deleted`
- `worker.updated`
- `space.created`
- `ink.created`
- `ink.updated`
- `ink.deleted`
- `scene_lock.acquired`
- `scene_lock.heartbeat`
- `scene_lock.released`
- `scene_lock.stale`

WebSocket 建立时只发送 `realtime.ready`，不发送完整 Project snapshot。客户端 Connected / Reconnected 后执行 REST workspace GET，再处理后续增量事件。

## 17. Database Migration

数据库 schema version 为 `2`。迁移逻辑位于 `CrystalServer/app/database.py::migrate`，采用：

1. Phase 1 无版本表时写入 version 1。
2. 添加 Phase 2 字段和 detail / relationship tables。
3. 恢复旧 description、Scene detail、Lock alias、View revision 等数据。
4. 写入 version 2。
5. 由 server-owned、幂等 seed 补齐演示 fixture。

`tests/test_phase2_model.py::test_phase1_database_migrates_without_data_loss` 已验证旧 Project、Space、Entity、View、Scene Lock 保留。

## 18. API 列表

项目与空间：

- `GET /api/v1/health`
- `GET /api/v1/projects`
- `GET /api/v1/projects/{project_id}`
- `GET /api/v1/projects/{project_id}/spaces`
- `POST /api/v1/projects/{project_id}/spaces`
- `GET /api/v1/projects/{project_id}/workspace?space_id=...`
- `PATCH /api/v1/projects/{project_id}/spaces/{space_id}/camera`
- `GET /api/v1/projects/{project_id}/search?q=...`

Entity / View / Relationship：

- `POST/GET/PATCH/DELETE /api/v1/entities[/{entity_id}]`
- `PUT/DELETE /api/v1/entity-views/{entity_id}/{space_id}`
- `POST /api/v1/relationships`
- `GET /api/v1/projects/{project_id}/relationships`
- `DELETE /api/v1/relationships/{relationship_id}`

Ink / Lock / Realtime：

- `POST/DELETE/PATCH /api/v1/ink-strokes[/{stroke_id}]`
- `GET /api/v1/projects/{project_id}/scene-locks`
- `GET /api/v1/scene-locks/{scene_id}`
- `POST /api/v1/scene-locks/{scene_id}/acquire`
- `POST /api/v1/scene-locks/{scene_id}/heartbeat`
- `POST /api/v1/scene-locks/{scene_id}/release`
- `WS /ws/projects/{project_id}`

## 19. C++ 测试结果

- `cmake --build vs_build --config Debug --target crystal_core_tests --parallel 1`：PASS。
- `crystal_core_tests`：`Crystal core checks=15 status=PASS`。
- Crystal / UI2D 定向 CTest：21/21 PASS。
- `cmake --build vs_build --config Debug --target crystal_client --parallel 1`：PASS。
- 完整仓库 CTest 运行到 46 项时有 7 个既有 AudioRuntime / VoiceMixer 可执行文件缺失，属于未生成目标，不是 Crystal 测试失败；因此未将完整仓库结果写成 PASS。

## 20. Python / Server 测试结果

- `python -m compileall -q app tests run_server.py`：PASS。
- `pytest ... test_phase2_model.py test_scene_lock_service.py test_server.py`：`6 passed, 1 skipped`。
- 已覆盖并实际执行：schema / seed / Multi View fixture、Phase 1 migration、Relationship 唯一约束、Scene Lock 100 竞争、Wrong Owner、STALE 保护。
- `test_phase2_server.py` 和 Phase 1 FastAPI TestClient 模块因导入 FastAPI 失败而整模块跳过。

## 21. FastAPI Integration Test 状态

`FastAPI Integration: SKIPPED`

原因：当前 `F:\Python` 环境没有可导入的 FastAPI / TestClient 依赖；此前依赖安装受到网络波动影响，未伪造 PASS，也未让该环境问题阻塞 Phase 2 其他实现。

## 22. Scene Lock 100 次竞争复验结果

`Scene Lock Concurrent Acquire 100 Runs: PASS`

`test_scene_lock_service_concurrent_acquire_100_competitors` 结果为 100 个并发 Acquire 中 1 个成功、99 个 `AlreadyLocked`，成功者的 owner 与最终数据库 owner 一致。

## 23. 双客户端实时同步结果

`Two Client Realtime Sync: NOT TESTED`

原因是 FastAPI WebSocket TestClient 未安装。增量事件广播实现和测试用例已存在；当前可确认的是协议路径不再发送完整 workspace，客户端具有重连 REST authoritative reload 路径。

## 24. Revision Conflict 测试结果

`Revision Conflict Protection: NOT TESTED`

原因同上：HTTP 集成测试未能启动。服务端实现了事务内 revision 检查、409 payload 和当前 Entity 返回；C++ 客户端已按当前 revision 提交更新。

## 25. Multi View Same Entity 测试结果

`Multi View Same Entity: PASS`

模型测试确认 P_HighSchool 的 `P_HighSchool:task-npc-scene` 只有一个 Entity、三个 View；C++ serialization test 确认多个 `space_id` View 反序列化后仍只保留一个 Entity。

## 26. 修改文件列表

服务端：

- `CrystalServer/app/database.py`
- `CrystalServer/app/entities.py`
- `CrystalServer/app/main.py`
- `CrystalServer/app/services/scene_locks.py`
- `CrystalServer/tests/test_phase2_model.py`
- `CrystalServer/tests/test_phase2_server.py`
- `CrystalServer/tests/test_server.py`

C++ Crystal：

- `Source/Model/CrystalModels.h/.cpp`
- `Source/Net/CrystalApiClient.h/.cpp`
- `Source/Net/CrystalRealtimeClient.h/.cpp`
- `Source/Workspace/CrystalWorkspace.h/.cpp`
- `Source/Workspace/ProjectController.h/.cpp`
- `Source/Ink/CrystalInkController.h/.cpp`
- `Source/UI/CrystalRenderer.cpp`
- `Source/Main.cpp`
- `Tests/CrystalCoreTests.cpp`

Phase 2 使用的已有通用能力保持在 Engine：Selection、Tool routing、InfiniteCanvas、Ink anchor 字段和 Network client；TaskStatus、Worker、SceneLock、Relationship、Milestone、Kanban 均留在 `XiE_Projects/Crystal/`。

## 27. 未完成项目

- FastAPI / TestClient 依赖未恢复，Revision HTTP、Server CRUD 集成、WebSocket 单客户端广播和双客户端同步未实际跑通。
- 因此不能宣称 Demo C / Demo D 的双客户端端到端 PASS。
- C++ WebSocket 回调目前直接更新 Workspace；生产化版本应增加主线程事件队列，避免渲染线程与网络线程同时读写模型。
- 当前 MVP 无复杂权限、认证、CRDT、自动布局、Ink 协同编辑和自动释放 Scene Lock，符合 Phase 2 禁止项。
- Connect 的类型选择是键盘循环，不是复杂弹窗；后续可替换成通用轻量选择器。

## 28. 下一阶段建议

1. 网络恢复后安装 FastAPI / TestClient，执行 `test_phase2_server.py`，补跑 Revision、CRUD、WebSocket 和双客户端验证。
2. 将网络事件统一投递到 UI 主线程，再处理 reconnect authoritative reload。
3. 增加通用 Inspector field binding / inline editor，减少 Crystal Main 中的临时输入处理。
4. 在不复制 Entity 的前提下完善 Space 导航、聚焦 View、关系跳转和更细的 Task / Worker 操作反馈。
