# Crystal Phase 5 Delivery Report

日期：2026-08-11  
项目：Crystal / P_HighSchool  
构建：Debug，Windows 10 x64，OpenGL/GLFW，窗口 1280×760

## 1. Phase 5 总体结论

Phase 5 已完成网络传输修复、Debug 断线注入、Workspace UX 代码补齐、语义主题色收敛、真实窗口大型 Fixture Profiling，以及对应的 C++/Server 回归。

但本阶段没有达到“完整人工验收通过”：真实双客户端的 Test B 断线恢复已实测，Test A 只完成了服务端 REST 变更到客户端的实时观察；Test C/D 的断线期间四类修改、旧快照覆盖保护和 Scene Lock 复核没有完整执行。P_HighSchool 全量人工 UX 流程、自由绘制交互也没有在干净桌面上完整走完。因此总体结论为 FAIL，不能宣称日常使用标准已达成。

## 2. Real Reconnect E2E 流程和结果

真实运行了：

1. Crystal Server：`CrystalServer/.venv/Scripts/python.exe -m uvicorn app.main:app --host 127.0.0.1 --port 8787`。
2. 两个 Debug `crystal_client`，均连接 `P_HighSchool`。
3. Server 日志出现两个真实 WebSocket `[accepted] / connection open`，不是 Mock；`websockets==14.1` 已加入服务端依赖。
4. 服务端 REST 将 `P_HighSchool:task-npc-scene` 的 Status 从 Ready 改为 Testing，客户端 A 在真实窗口中平移到卡片后看到 `TESTING`；证据：`phase5-A-after-status.png`。
5. 暂停 Server 后，客户端 A 真实显示 `RECONNECTING`；证据：`phase5-A-server-paused.png`。
6. 重启 Server 后，A/B 重新显示 `CONNECTED`，并发生 authoritative workspace reload；证据：`phase5-A-after-server-restart.png`、`phase5-B-after-server-restart.png`。

结果拆分：

- Test A 正常连接与服务端变更到客户端：部分 PASS；未完成“先由 A UI 修改、再由 B UI 修改”的完整双向矩阵。
- Test B 真实断线：PASS；确实经历了 `CONNECTED → RECONNECTING → CONNECTED`。
- Test C 断线期间由 B 修改 Status、Relationship、Scene Lock、View/Board，再由 A 合并：NOT TESTED。
- Test D Scene Lock 权威状态恢复：NOT TESTED。
- Debug-only `Ctrl+Shift+D` 断线入口已实现，调用正式 WebSocket close/reconnect 路径，不修改业务数据；没有把该入口单独冒充完整 E2E 通过。

`Real Reconnect E2E: FAIL`（Test C/D 未完整执行）。

## 3. P_HighSchool 人工 UX Checklist

已实际打开 P_HighSchool 的 Crystal 桌面窗口，并完成真实连接、真实窗口平移和服务端实时状态观察。由于验收期间桌面上存在抢占焦点的全屏程序，直接鼠标 UI 操作不稳定；以下未完成项目均明确记为 NOT TESTED：

- Project / Space：切换、创建、Rename、Duplicate、Delete、Recent Space 及 Camera/Zoom/Views/Ink 恢复：NOT TESTED。
- Custom Entity Bug：Status、Severity、Priority、Owner、RelatedSystem、Description、Tags 表单完整操作：NOT TESTED。
- 临时 `Test Asset` Schema 及实体：NOT TESTED。
- Board `NPC Architecture Test` 的 Rename、Drag、Resize、Collapse/Expand、Lock/Unlock、多 Node attach、drag-out detach：NOT TESTED。
- Ink Pen、Highlighter、Arrow、Rectangle、Ellipse、Group/Move/Duplicate/Ungroup/Delete、Board Anchor：NOT TESTED。
- Layer `UX Test` 的 Create/Rename/Active/Visibility/Lock/Reorder/Delete 及视觉对象迁移：NOT TESTED。
- Query `Task / Status Equals InProgress` 的 Preview、保存和 Dashboard Live Update：NOT TESTED。
- Dashboard 五类 Widget 的创建、配置、Drag、Resize、Duplicate、Delete、Inspector：NOT TESTED。
- Scene Lock Acquire/Release、第二客户端冲突提示和双方 FREE：NOT TESTED。
- Presentation Mode：NOT TESTED。

因此不能以代码路径或单张连接截图替代完整人工流程。

## 4. Layer CRUD

已补齐 `+ NEW LAYER`、inline Rename、Active Layer、Visibility/Lock 行为、Reorder 菜单和 Delete 时将视觉内容移动到 Main 的安全路径；Layer Rename 会同步 View、Board、Ink、Widget 的 layer 引用，服务端也做了 canonical layer name migration。

C++ 状态和服务端路径有回归覆盖，但真实桌面上的完整 Create/Rename/Visibility/Lock/Reorder/Delete 流程未执行。

结论：实现 PASS；人工 UX 验收 NOT TESTED。

## 5. Widget Inspector

已补齐 Widget 选中后的 Inspector 展示、Title inline Rename、Layer、Width/Height、Config 序列化及 Context Menu 的 Open Inspector/Move/Duplicate/Delete 操作路径。Widget config 使用结构化 JSON round-trip，不要求用户直接编辑 JSON。

真实窗口中未完成 EntityList、Counter、Progress、RecentActivity、SceneLockList、WorkerList 的逐项表单编辑验收。

结论：实现 PASS；人工 UX 验收 NOT TESTED。

## 6. Schema CRUD

已补齐 Schema Editor 的创建、Rename、Draft Property Add/Delete、Property Type/Required/Display Name 数据路径，以及 Schema/Property REST CRUD；已有数据占用时仍由服务端保护危险 Type/Enum Option 修改。

`Test Asset` 的 Status Enum、Owner EntityReference、Tags、Progress、AssetPath 组合没有在真实桌面中完整创建实体并删除清理。

结论：Server/model PASS；人工表单验收 NOT TESTED。

## 7. Board Highlight / Auto Detach

已实现：

- 拖拽 View 经过 Board 时维护 hover board state。
- 可 attach 的 Board 显示 `RELEASE TO ATTACH`，Locked Board 显示 `LOCKED` 反馈。
- 在 Board 外释放时 detach，并保留世界坐标及完整 View 字段。
- Board Rename、Collapse/Expand、Lock/Unlock、Move To Layer、Duplicate、Delete 操作路径已接入。

C++ 已覆盖 hover/locked state 和 auto-detach full payload；真实鼠标拖入/拖出 Board 的桌面验收未执行。

结论：实现/单测 PASS；人工验收 NOT TESTED。

## 8. Context Menu

已将高频提示路径改为真实操作：

- View：Open Inspector、Duplicate、Attach、Detach、Move To Layer、Add View To Space、Delete。
- Board：Rename、Move To Layer、Lock/Unlock、Collapse/Expand、Duplicate、Delete。
- Ink：Group、Ungroup、Duplicate、Move To Layer、Delete。
- Widget：Open Inspector、Rename、Move To Layer、Duplicate、Delete。
- Layer：Rename、Move Up、Move Down、Delete。

Context Menu state 和 inline edit 有 C++ 回归；真实桌面逐项点击验收未完成。

## 9. Theme Token 收敛

在 `CrystalThemeTokens` 中增加并使用了 Hover、Stale、Disabled 语义色，并将连接状态、Locked/Layer Locked、Selected、Board Highlight、Inspector、Layer 面板、通知、Scene Lock 等状态色迁移到 token。数据可视化本身的特殊颜色保留，未做无价值的 100% 字面量重构。

结论：PASS；`crystal_client` Debug 编译通过。

## 10. Large Workspace 测试机器环境

- OS：Windows 10 专业版 10.0.19045，64-bit。
- CPU：Intel Core i7-10700F，8 cores。
- RAM：约 31.9 GB。
- 构建：MSBuild 17.14.23，CMake Debug。
- 窗口：真实 GLFW/OpenGL 窗口 1280×760。
- Server：本机 `127.0.0.1:8787`。
- Profiler 输出：`vs_build/XiE_Projects/Crystal/Debug/CrystalProfile.csv`。

## 11. Large Workspace fixture

使用 Debug `--fixture` 真实打开桌面窗口，Fixture 精确构造：

- 300 Entity Views
- 100 Relationships
- 500 Ink Strokes
- 20 Boards
- 20 Dashboard Widgets

fixture 加载在 realtime Connected 后执行，避免初始 REST reload 覆盖测试数据。

## 12. Pan profiling

Profiler 类别：`pan`。真实窗口收集到 1 个平移样本：平均 0.0012 ms，最大 0.0012 ms。样本量较小，但没有观察到输入延迟；长期平移 UX 仍应在干净桌面补测。

结论：Large Workspace Pan/Zoom 中 Pan 部分 PASS（有限样本）。

## 13. Zoom profiling

Profiler 类别：`zoom`。真实窗口收集 6 个 Zoom In/Out 样本：平均 0.134667 ms，最大 0.8067 ms。没有观察到明显卡顿。

## 14. HitTest profiling

Profiler 类别：`hittest`。真实窗口收集 4 个命中样本：平均 0.125775 ms，最大 0.2399 ms。HitView/HitBoard/HitWidget 现在会过滤当前 Space、Visible、Hidden Layer 和 collapsed Board，并使用统一世界矩形边界 helper。

结论：Large Workspace HitTest: PASS（已测样本有限；长时间快速移动/点击仍建议在干净桌面复验）。

## 15. Draw profiling

Profiler 类别：`draw`。真实窗口静止运行大型 Fixture，稳定窗口帧 Draw 平均约 16.42–16.49 ms；第一个 flush 周期为 16.8042 ms，存在一次 107.761 ms 的启动/切换峰值，后续 240–960 帧窗口最大值约 17.48–18.69 ms。

自由绘制覆盖既有 500 Stroke 的连续 Pen/Highlighter/Shape 操作没有完整采样，因此 Draw 交互验收不能写 PASS。

## 16. 实际发现的性能瓶颈

优化前真实 Fixture 的 Draw 平均约 75.45–76.86 ms/frame，主要原因是视口外的 300 View、500 Ink、20 Board、20 Widget 和 Relationship 仍进入了昂贵绘制准备。Realtime 本身不是瓶颈，稳定样本约 0.01 ms。

HitTest、Pan、Zoom、Inspector 样本没有显示持续性瓶颈；没有证据表明 Query 每帧完整重算，因此没有为 Query 引入额外缓存重构。

## 17. 实际进行的性能优化

依据上述数据实施了轻量 Visible Bounds Culling：

- Entity View：世界矩形裁剪。
- Board：世界矩形裁剪，兼容 collapsed header 高度。
- Widget：世界矩形裁剪。
- Ink：包含 anchor 偏移的 Stroke bounding box 裁剪。
- Relationship：端点线段 bounds 裁剪。
- HitTest：复用统一世界矩形判定，并增加 Layer/Space/Visibility/Collapsed 过滤。

没有引入 ECS、复杂 Quadtree 或第三方 profiler。

## 18. 优化前后数据

| 场景 | 优化前 | 优化后 | 结果 |
|---|---:|---:|---|
| Large Fixture Draw，启动/初始周期 | 约 75–77 ms/frame | 16.8042 ms/frame | 明显改善 |
| Large Fixture Draw，稳定窗口 | 约 75–77 ms/frame | 16.42–16.49 ms/frame | 接近 60 FPS 理想预算 |
| Realtime，稳定窗口 | 约 0.009–0.012 ms | 约 0.010–0.013 ms | 非瓶颈 |
| Pan | 未有旧基线 | 0.0012 ms | 无明显延迟 |
| Zoom | 未有旧基线 | avg 0.134667 / max 0.8067 ms | 无明显延迟 |
| HitTest | 未有旧基线 | avg 0.125775 / max 0.2399 ms | 无明显延迟 |
| Inspector | 未有旧基线 | avg 0.006 / max 0.0152 ms | 无明显延迟 |

## 19. Undo / Redo 人工验证

代码和 core 状态测试覆盖了 Move Node、Move Board、Resize Board、Ink Group Transform、Duplicate/Delete 的本地 history 路径；realtime 外部事件不进入本地 Visual Undo History。

本阶段没有在干净真实 Workspace 中逐项执行 Ctrl+Z/Ctrl+Y 并记录人工结果，因此人工结论：NOT TESTED。

## 20. 输入 Focus 验证

代码保留 TextInput active 优先级，Context Menu、Property Picker、Query/Schema Editor 和 inline edit 都有单独输入分支；C++ core 保留了文本编辑/快捷键相关覆盖。

本阶段没有在真实桌面逐项验证输入 `P` 不切换 Pen、Popup 焦点和 Context Menu 键盘操作，因此人工结论：NOT TESTED。

## 21. 异常操作验证

服务端和模型路径覆盖了 Layer Locked 写入拒绝、Schema property/type/enum 使用保护、删除 Space 不删除 Entity、Board/Stroke/View 数据完整性等场景。

以下桌面异常操作没有完整人工执行：断线 Acquire、Locked Board Resize、Locked Layer Delete、Attach 到 Locked Board、Paste 到不存在 Space、删除 Board 后 anchored Ink、重复 View 等。结论：NOT TESTED，不能以代码路径替代人工 PASS。

## 22. C++ Tests

- `ctest --test-dir vs_build -C Debug --output-on-failure -R crystal_core_tests`：PASS。
- `crystal_core_tests.exe`：`Crystal core checks=57 status=PASS`。
- 新增/覆盖：Auto Detach、Board Hover/Locked state、Layer rename/CRUD state、Widget Inspector serialization、Schema model、Context Menu/inline edit、Visible culling geometry、HitTest rectangle filtering、Large Fixture model traversal。
- 完整 CTest：39/46 PASS，7 个测试未运行，原因是现有构建目录缺少与 Crystal 无关的 AudioRuntime/VoiceMixer executable：`xie_parametric_eq_spectrum_tests`、`xie_noise_suppression_tests`、`xie_haze_effects_tests`、`xie_synthetic_voice_tests`、`xie_electronic_mist_tests`、`xie_voicemixer_tests`、`xie_voicemixer_ui_smoke`。

## 23. Server Tests

- `python -m py_compile app/main.py app/database.py app/entities.py tests/test_phase5_server.py`：PASS。
- 工作区 basetemp 下 `python -m pytest -q -p no:cacheprovider tests`：25 passed。
- `pip check`：No broken requirements found。
- `tests/test_phase5_server.py` 的 Schema CRUD、Layer canonicalization/rename migration、Delete safety 等覆盖已包含在上述回归中。

第一次直接运行受用户 Temp 目录 ACL 影响，pytest 无法扫描 `C:\Users\xiling\AppData\Local\Temp\pytest-of-xiling`；改用工作区 basetemp 后完整测试通过。这是测试环境问题，不是 Server assertion failure。

## 24. Scene Lock 100 concurrency

Scene Lock service 的 100 competitor 测试通过；server integration 的 `test_concurrent_acquire_has_exactly_one_success` 在工作区 basetemp 下通过。测试断言 100 次并发 Acquire 只有 1 次成功，其余请求被拒绝，锁语义未因 UX 改动而改变。

结论：Scene Lock Concurrent Acquire 100 Runs: PASS。

## 25. 修改文件列表

Phase 5 相关修改主要位于：

- `Engine/Network/WebSocketClient.h/.cpp`
- `XiE_Projects/Crystal/Source/Main.cpp`
- `XiE_Projects/Crystal/Source/Net/CrystalApiClient.h/.cpp`
- `XiE_Projects/Crystal/Source/Net/CrystalRealtimeClient.h/.cpp`
- `XiE_Projects/Crystal/Source/UI/CrystalRenderer.cpp`
- `XiE_Projects/Crystal/Source/UI/CrystalTheme.h`
- `XiE_Projects/Crystal/Source/UI/CrystalProfiler.h`
- `XiE_Projects/Crystal/Source/UI/CrystalRenderGeometry.h`
- `XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.h/.cpp`
- `XiE_Projects/Crystal/Tests/CrystalCoreTests.cpp`
- `XiE_Projects/Crystal/CrystalServer/app/main.py`
- `XiE_Projects/Crystal/CrystalServer/requirements.txt`
- `XiE_Projects/Crystal/CrystalServer/tests/test_phase5_server.py`
- `XiE_Projects/Crystal/CrystalPhase5DeliveryReport.md`

本阶段没有执行 Git、Commit、Branch、CI 或 GitHub 操作。

## 26. 已知问题

1. Test C/D 的真实断线期间多对象修改和 stale snapshot 顺序没有完成验收。
2. P_HighSchool 全量桌面 UX 因桌面焦点干扰未完成；代码实现、截图和 core/server 测试不能替代这项人工验收。
3. 大型 Fixture 的 Box Selection、Layer Toggle、连续自由绘制尚未取得独立 profiling 样本。
4. Visible culling 仍需遍历模型以做轻量过滤；当前数据下已达到约 16.4 ms 稳态，不需要引入复杂空间索引。
5. 全仓 CTest 的 7 个未运行项属于当前构建目录缺少的 AudioRuntime/VoiceMixer 二进制，与 Crystal Phase 5 无关。

## 27. 是否达到日常使用标准

没有达到。性能方面真实窗口的 Pan/Zoom/HitTest/稳定 Draw 已达到可接受水平，Server/C++ 回归也通过；但“真实 Reconnect 全流程 + P_HighSchool 完整人工 UX + 大型 Workspace 完整交互”三项没有全部完成，所以 Daily Usability Baseline 必须为 FAIL。

本阶段到此停止，不自行进入 Phase 6 实现。

## 28. Phase 6 建议

Phase 6 如需继续，优先在干净桌面环境重复并完成：

1. Test A 的真实 A→B、B→A UI 修改矩阵。
2. Test C 的 Status、Relationship、Scene Lock、View/Board 断线修改与 stale snapshot 顺序检查。
3. Test D 的 Scene Lock 权威恢复与第二客户端冲突提示。
4. P_HighSchool 全量人工 Checklist，包括 Layer、Widget、Schema、Ink、Undo/Redo、Focus、异常操作和 Presentation。
5. 在相同 Fixture 下补采 Box Selection、Layer Toggle、连续 Draw 样本。

以上是建议，不代表本阶段已完成这些验收。

Daily Usability Baseline: FAIL
