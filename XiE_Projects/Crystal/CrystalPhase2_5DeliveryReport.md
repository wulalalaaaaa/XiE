# Crystal Phase 2.5 交付报告

日期：2026-08-10

目标：把 Phase 2 已有的联网架构稳定化，建立主线程实时事件队列，补齐重连 authoritative reload、snapshot / realtime 竞态保护、Scene Lock heartbeat、连接状态和错误反馈。

## 1. FastAPI 环境处理方式

按照指令先检查了全局 Python 和项目独立环境：

- 全局：`F:\Python\python.exe`，Python 3.13.5，已有 `httpx 0.28.1`、`pytest 8.3.4`，缺少 FastAPI、Starlette、Uvicorn。
- 项目环境：`XiE_Projects/Crystal/CrystalServer/.venv`，Python 3.13.5，原先没有可用依赖。
- 未向全局 Python 安装包。
- 先在 `.venv` 使用默认索引安装，返回没有可用 FastAPI 版本；随后使用官方 PyPI 重试，两个网络路径均超时。
- 没有修改服务端业务逻辑来绕过集成测试。

`requirements.txt` 已改为项目依赖锁定文件，版本如下：

```text
fastapi==0.115.6
starlette==0.41.3
uvicorn==0.34.0
pytest==8.3.4
httpx==0.28.1
```

## 2. 实际依赖列表

Crystal Server 运行和测试实际需要：FastAPI、Starlette、Uvicorn、HTTPX、Pytest，以及 Python 标准库中的 SQLite / asyncio / concurrent.futures。

由于网络索引不可用，本轮没有伪造安装成功；锁定版本已写入 `CrystalServer/requirements.txt`，待网络恢复后可直接在 `.venv` 执行安装。

## 3. 主线程事件队列架构

新增通用 Engine 头文件 `Engine/Core/MainThreadEventQueue.h`：

```text
Network Thread
    ↓
CrystalRealtimeClient
    ↓  enqueue(CrystalRealtimeEvent)
Engine::MainThreadEventQueue<T>
    ↓  Drain()，FIFO
CrystalWorkspace::PumpRealtimeEvents()
    ↓
CrystalWorkspace::ApplyRealtimeEvent()
    ↓
UI / Renderer 读取 Workspace
```

队列具备 thread-safe enqueue、主线程 drain、FIFO、move-friendly payload；Engine 只知道模板类型，不知道 Task、Worker、SceneLock 或 CrystalEntity。

## 4. WebSocket callback 改造

`CrystalRealtimeClient` 的 WebSocket message/state callback 现在只做：

- JSON 解析和事件类型分类。
- 将 `CrystalRealtimeEvent` 放入线程安全队列。

网络 callback 不再直接调用 Workspace，不再修改 Entity、View、Relationship、Ink、Lock 或 UI 状态。`realtime.ready` 仍被忽略，Phase 2 的增量事件全部进入队列。

## 5. ApplyRealtimeEvent 统一入口

新增公开主线程入口：

```cpp
CrystalWorkspace::PumpRealtimeEvents()
CrystalWorkspace::ApplyRealtimeEvent(const CrystalRealtimeEvent&)
```

入口统一处理：

- Entity created / updated / deleted
- View created / updated / deleted
- Relationship created / deleted
- Space created
- Worker updated
- Ink created / updated / deleted
- Scene Lock acquired / heartbeat / released / stale
- Connection state
- 兼容的 workspace snapshot

本地 API 成功响应也转换为同样的 realtime event 进入 `ApplyRealtimeEvent`，减少本地更新与远程更新的分叉。

## 6. Reconnect 流程

连接状态现在明确为 `Connected`、`Connecting`、`Reconnecting`、`Disconnected`；WebSocket 曾经成功连接后再次尝试时会进入 `Reconnecting`。

收到 `Connected` 事件后，主线程执行：

1. 保持 Connecting / Reconnecting 状态。
2. GET 当前 Project / Space 的 REST authoritative workspace。
3. 应用 snapshot，刷新 Ink、Lock、Worker 摘要和选中状态。
4. Drain 连接建立期间排队的增量事件。
5. 按 FIFO 应用这些事件。
6. 最终切换为 Connected；reload 失败则切换为 Disconnected 并显示提示。

客户端没有尝试通过补收 WebSocket 消息代替 REST authoritative reload。

## 7. Snapshot / realtime race 处理方式

`PumpRealtimeEvents()` 在处理 Connected 时会先完成 REST reload，再 drain 期间新到的事件，避免旧 snapshot 后覆盖已排队事件。

另外，Entity / View 的替换逻辑增加 revision 单调保护：较低 revision 的事件或 snapshot 不会覆盖当前较高 revision。`ApplyAuthoritativeSnapshot()` 在同 Project 场景下保留已有的更高 Entity / View revision。

C++ 测试模拟了：当前 revision 6，随后到达 revision 5 snapshot，最终仍为 revision 6。

## 8. Revision Conflict 实测

服务端集成测试已补充检查：

- A 使用 revision 1 更新成功为 revision 2。
- B 继续使用 revision 1 得到 HTTP 409。
- 返回 `revision_conflict`。
- 返回 `expected_revision`、`current_entity`、current revision 2 和 A 的状态修改。

但由于 FastAPI / TestClient 未安装，该 HTTP 测试本轮未执行；C++ 客户端已经实现 409 处理：应用服务器返回的 authoritative Entity，并显示 `CONFLICT: latest server Entity applied`，不会静默覆盖。

## 9. 双客户端实时同步实测

`test_phase2_server.py` 新增了两个 WebSocket 客户端的集成流程，覆盖：

- Entity Status：InProgress → Testing
- Priority：Normal → High
- Relationship create / delete
- View create
- Scene Lock acquire / duplicate acquire / release
- Scene Lock heartbeat

由于 FastAPI 依赖安装失败，本轮没有启动 Crystal Server，也没有执行双客户端 HTTP/WebSocket 实测。

## 10. Scene Lock heartbeat

Phase 1 的原子锁逻辑未修改，仍使用 `BEGIN IMMEDIATE`，STALE 不会自动变为 FREE。

客户端新增本地持有 Lock 记录，主线程每 20 秒调用 heartbeat API；成功响应通过 `ApplyRealtimeEvent` 更新 Lock。WebSocket parser 现在接收 `scene_lock.heartbeat`，其他客户端可刷新 heartbeat 时间。

## 11. Connection State

HUD 不再只显示 bool，当前明确显示：

- Connected
- Connecting
- Reconnecting
- Disconnected

Scene Acquire、Heartbeat、Release、Entity / View / Relationship / Ink 写操作和搜索在非 Connected 状态下返回 `server_disconnected`，并显示轻量状态提示，不会伪造本地联网成功。

## 12. 错误反馈

增加了固定 HUD 状态提示，覆盖以下类别：

- `SCENE ALREADY LOCKED`
- `CONFLICT: latest server Entity applied`
- `SERVER DISCONNECTED`
- `RECONNECTING`
- `RELATIONSHIP DUPLICATE`
- `INVALID ENTITY`
- 通用 `REQUEST FAILED`

没有引入复杂 Toast 或新的 Widget System。

## 13. C++ 测试

- `crystal_core_tests`：`Crystal core checks=20 status=PASS`。
- 新增 MainThreadEventQueue 多线程生产、FIFO drain、清空检查。
- 新增 `entity.updated` 统一入口检查。
- 新增 Relationship 和 Scene Lock 统一入口检查。
- 新增 snapshot revision 防回退模拟检查。
- Crystal / UI2D 定向 CTest：21/21 PASS。
- `crystal_client` Debug 构建：PASS。

完整仓库仍有既有 AudioRuntime / VoiceMixer 可执行目标未生成，因此未把完整仓库 CTest 写成全绿；这不属于本轮 Crystal 目标失败。

## 14. Python / Server 测试

实际使用全局现有 `F:\Python` 仅运行不依赖 FastAPI 的检查：

- `python -m compileall -q app tests run_server.py`：PASS。
- Model / Scene Lock 测试：6 passed。
- Phase 1 Server TestClient 模块：SKIPPED，缺少 FastAPI。
- Phase 2 Server TestClient 模块：SKIPPED，缺少 FastAPI。
- 合并命令结果：`6 passed, 2 skipped`。

已保留并继续覆盖 Scene Lock 100 并发、Wrong Owner、STALE 保持占用等测试。

## 15. 手动双客户端测试

`NOT TESTED`。

原因：FastAPI / Uvicorn 依赖下载失败，无法启动 Crystal Server；因此没有启动两个 `crystal_client` 执行真实联网验收，也没有伪造 Client A / Client B 的 PASS。

## 16. 修改文件列表

通用 Engine：

- `Engine/Core/MainThreadEventQueue.h`
- `Engine/Network/WebSocketClient.cpp`

C++ Crystal：

- `XiE_Projects/Crystal/Source/Net/CrystalRealtimeEvent.h/.cpp`
- `XiE_Projects/Crystal/Source/Net/CrystalRealtimeClient.h/.cpp`
- `XiE_Projects/Crystal/Source/Model/CrystalModels.h/.cpp`
- `XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.h/.cpp`
- `XiE_Projects/Crystal/Source/UI/CrystalRenderer.cpp`
- `XiE_Projects/Crystal/Source/Main.cpp`
- `XiE_Projects/Crystal/Tests/CrystalCoreTests.cpp`

服务端 / 测试 / 配置：

- `XiE_Projects/Crystal/CrystalServer/requirements.txt`
- `XiE_Projects/Crystal/CrystalServer/README.md`
- `XiE_Projects/Crystal/CrystalServer/tests/test_phase2_server.py`

## 17. 未完成项

- FastAPI 依赖受网络环境影响未安装，所有 FastAPI TestClient、真实 Server、双客户端和 Uvicorn 手动测试均未执行。
- Revision Conflict Protection 的服务端端到端结果为 NOT TESTED；实现、响应断言和 C++ 处理已完成。
- Two Client Realtime Sync 的端到端结果为 NOT TESTED；双客户端测试用例已完成。
- Reconnect Authoritative Reload 没有真实断线手测，只有 C++ 流程实现和 snapshot / pending event 模拟测试。
- 本轮没有进入 Phase 3，没有新增 Entity 类型、权限、Chat、AI、OCR、自动布局、Git 或 CI。

## 最终验收状态

`FastAPI Integration: NOT TESTED`

`Revision Conflict Protection: NOT TESTED`

`Two Client Realtime Sync: NOT TESTED`

`Reconnect Authoritative Reload: NOT TESTED`

`Snapshot / Realtime Race Protection: PASS`

`Main Thread Realtime Queue: PASS`

`Scene Lock Concurrent Acquire 100 Runs: PASS`
