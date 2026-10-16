# Crystal Phase 4 Delivery Report

日期：2026-08-11  
范围：Workspace UX 生产化与视觉完成度第一版  
状态：代码实现与自动化验证完成；真实断线和完整人工视觉验收未伪造为通过。

## 1. Phase 4 总体完成情况

Phase 4 已完成第一版 Workspace UX 主链路：Property Editor、实体选择、框选/多选、Ink/Board/Widget 视觉操作、Layer Lock、Query/Schema 面板、Context Menu、内部 Clipboard、Undo/Redo、主题 Token、Display Mode 和 Fit/Focus 已接入现有 Crystal Workspace。

服务端补充了 Number 约束、Schema 安全修改、Board Collapse、Stroke 完整 Patch 和 Layer Lock 写保护。服务端 schema version 为 4。

自动化结果：

- C++ core checks：48 PASS
- `crystal_client` Debug：PASS
- Crystal Server pytest：23 passed
- `py_compile app/main.py app/database.py app/entities.py`：PASS

真实 Reconnect E2E、P_HighSchool 全流程人工 UX 验收和完整 GUI 性能测量仍标记为未测试。

## 2. Property Editor

新增 `PropertyEditor` 统一处理 Text、MultilineText、Number、Bool、Enum、Date、URL、EntityReference、EntityReferenceList、Tags、Progress。

Number 支持 min/max/step；Progress 固定校验 0–100；Enum 非法值拒绝；引用字段校验当前 Project 中的实体 ID；Tags 会去重；客户端错误显示字段级 `PROPERTY: ...` 提示。

主 Inspector 已使用上述解析器处理 Custom Entity，Bool/Progress/Enum 有可视化反馈。Tags 和多行文本采用轻量 HUD 输入，不重写 XiE 文本系统。

## 3. Entity Picker

EntityReference 和 EntityReferenceList 字段点击后打开 Entity Picker，可按 Title、Type/Schema 搜索并选择实体；列表引用支持追加并去重，保存仍使用实体 ID。

View/Entity 的职责保持分离，Picker 不会通过输入 UUID 的方式绕过实体选择。

## 4. Generic Inspector

Custom Entity 继续通过 Schema 自动生成 Inspector，没有新增 BugInspector、DecisionInspector 或 ArtAssetInspector。服务器返回的属性约束错误通过字段对应的客户端提示展示；内置 Task、Worker、Scene、System、Milestone 业务 Inspector 保持兼容。

## 5. Selection System

`SelectionModel` 扩展为 EntityView、Board、InkStroke、InkGroup、DashboardWidget 五类选择项，支持单选、Shift 多选、Ctrl Toggle、空白框选和点击空白清除。

框选在 Screen/World 变换之间计算，包含 Node、Board、Stroke/Group 和 Widget；Collapsed Board 的可视区域只按 Header 参与命中。

## 6. Ink Group Transform

选中 Ink Group 后可整体移动、复制和删除；移动会保持每条 Stroke 的 shape、width、color、points，复制会生成新的 Stroke ID 和新的 group ID。父 Board 与 Child View 同时选择时通过 `movedBoards` 去重，避免 Child 移动两次。

Group 的现有服务端 API 和 Box Selection 已接通；Context Menu 中 Group/Ungroup 的完整编辑器仍属于后续细化项。

## 7. Layer Panel

左侧 Layer Panel 显示 active、visible 和 locked 状态；点击可切换可见性、锁定状态或 Active Layer。新 Stroke 使用 Active Layer，View/Board/Widget/Ink 的变换和删除都会检查 Layer Lock。

服务端对所有视觉写操作增加 Layer Lock 保护，锁定后仍可显示和查看，禁止写入。

## 8. Board Interaction

Board 支持选中、拖动、右下角 Resize、Collapse/Expand、Lock、Layer 和 Duplicate/Delete。拖动 View 进入 Board 区域后会按释放位置调用 Attach View To Board；Board 移动会同步 Child 的缓存位置并在服务端使用 board-local 坐标保持稳定。

Board Locked 后不能拖动、Resize、删除或 Collapse；Board 内部 View 仍可在其 Layer 未锁定时编辑。Detach 保留在 Workspace API 中，拖出 Board 的自动 Detach 仍是后续 UX 细化项。

## 9. Dashboard Widget Interaction

Dashboard Widget 已加入统一 Selection，可拖动、右下角 Resize、删除、Duplicate，并检查所属 Layer Lock。复制只复制 Widget 配置和视觉数据，不复制 Entity。

Widget 的 Query/Source 配置仍由服务器数据模型保存；右侧 Widget 专用 Inspector 的逐字段控件还需要下一阶段补齐。

## 10. Query Builder

新增 Query Builder 面板，支持显示 Entity Schema、AND Filter、Limit、实时 Matches 和最多 5 条结果预览；保存前修改保留在 Draft，Enter 才写入正式 Query View。

面板提供轻量键盘编辑：`I` 添加 Filter、`O` 循环 Equals/NotEquals/Contains、`U` 删除最后 Filter，直接输入编辑最后 Filter 值。

## 11. Schema Editor

新增 Schema Editor 面板，显示当前 Schema 的属性、类型和 Required 状态，并通过 Enter 保存。服务端禁止已有非空数据的 Property Type 直接改变，也禁止删除正在使用的 Enum option；对应错误返回 409 和明确 error code。

第一版没有扩展复杂 Project Settings 页面，Create/Rename/Property CRUD 的完整表单属于后续 UI 细化，但底层安全规则和 Schema serialization 已完成。

## 12. Scene Lock UX

现有 Scene Lock Widget 保持 FREE、LOCKED、STALE 三态语义；STALE 使用 Warning Token 显示，仍视为 LOCKED。Scene 选中后 Enter 继续执行服务器权威 Acquire/Release，失败不会先在本地伪造成功。

顶部连接状态显示 Connected、Reconnecting/Connecting、Disconnected；Scene Lock Concurrent Acquire 的 100 并发行为由服务端测试继续覆盖。

## 13. Context Menu

Canvas 已增加右键 Context Menu：

- View：Open Inspector、Duplicate View、Attach To Board 提示、Delete View、Delete Entity
- Board：Lock/Unlock、Collapse/Expand、Duplicate、Delete
- Ink：Duplicate、Move To Layer 提示、Delete
- Widget：Open Inspector 提示、Duplicate、Move To Layer 提示、Delete
- Blank：Create Note、Create Board、Paste

命令由 CrystalWorkspace 执行，Renderer 仅绘制菜单。尚未完成的菜单项会显示操作提示，不会静默破坏数据。

## 14. Clipboard

实现 Crystal 内部 Clipboard，覆盖 View、Board、Ink、Widget。View Copy 默认只复制 View；同一 Space 粘贴时不会创建第二个 Entity，也不会绕过服务器的 entity/space 唯一约束。Board、Stroke/Group、Widget 粘贴会生成新的视觉对象 ID。

服务端测试 `test_view_clipboard_lifecycle_never_deletes_entity` 验证删除 View 后 Entity 仍存在。

## 15. Undo / Redo

新增通用 `WorkspaceCommandHistory`，当前覆盖本地视觉状态的 View/Board/Ink/Widget Move、Board/Widget Resize、视觉对象复制/删除前后的状态快照，以及键盘 Ctrl+Z/Ctrl+Y。

Undo/Redo 不接管 Scene Lock、Activity、Worker 状态和其他服务器权威外部变化；历史为当前客户端内存状态，属于第一版 Workspace UX 命令历史。

## 16. Display Modes

All、Graph、Draw、Presentation 保持循环切换。

- All：正常工作区
- Graph：保留实体/关系视图，Ink 不作为主内容
- Draw：Board 保留为参考背景，提供 Pen、Highlighter、Line、Arrow、Rectangle、Ellipse、Eraser 和有限颜色预设
- Presentation：隐藏 Inspector、Layer/Selection/编辑工具条和编辑面板，保留项目内容、连接状态和必要导航信息

新增 Fit Content 和 Focus Selection 快捷操作，分别对应 Home 和 F。

## 17. Theme / Visual Tokens

新增 `CrystalThemeTokens`，集中定义 Background、Surface、SurfaceElevated、TextPrimary、TextSecondary、Accent、Success、Warning、Error、Selection、Locked、Offline 以及 spacing/corner radius。

Renderer 的主要 HUD、状态、Selection、Board Lock、Scene Lock、Layer 和面板已迁移到 Token；旧业务绘制中仍有少量历史 RGB 常量，后续可继续收敛。

## 18. Real Reconnect E2E

NOT TESTED。

本阶段未伪造“断 Server/断 Client 网络→自动重连→REST authoritative reload→pending realtime event 收敛”的结果。当前代码保留现有 Reconnecting/Connected 状态与 authoritative snapshot/realtime 路径，但没有在本次环境中完成可复现的实际断线注入和双客户端 GUI 验收。

## 19. Large Workspace Performance

已新增确定性 synthetic fixture：300 Entity Views、100 Relationships、500 Ink Strokes、20 Boards、20 Widgets，并完成构造、空间过滤和视觉遍历检查。

自动化 fixture PASS；没有把它冒充为真实 GUI Pan/Zoom/HitTest/Inspector 帧率 Benchmark。完整大型 Space 的交互性能仍需在可操作桌面窗口中测量。

## 20. P_HighSchool 手动 UX 验证

NOT TESTED。

本次没有把 P_HighSchool 的 Project/Space 切换、Bug 创建、引用/Tags 编辑、Board 拖入、Ink Group、Layer、Query、Dashboard、Acquire/Release、Presentation 全流程标记为人工 PASS。代码路径已接入，仍应由桌面端进行一次真实操作验收。

## 21. C++ Tests

执行：

```text
cmake --build vs_build --config Debug --target crystal_core_tests --parallel 1
vs_build/XiE_Projects/Crystal/Debug/crystal_core_tests.exe
```

结果：`Crystal core checks=48 status=PASS`。

新增/保留覆盖包括 Number constraints、Enum Popup、Entity Reference/List、Tags、Progress、Multi-kind Selection、Box Selection model、Board/Child transform dedup、Ink Group Move、Ink Duplicate、Layer Lock policy、Board/Widget Resize、Query/Schema model、View/Entity separation、Board Collapse、Workspace Undo/Redo 和大型 fixture。

## 22. Server Tests

执行：

```text
.venv/Scripts/python.exe -m pytest -q tests
```

结果：`23 passed in 7.44s`。

新增 Phase 4 server coverage：Number min/max/step、Schema unsafe type change、Used Enum option delete reject、Layer Lock blocks View/Board/Widget/Ink writes、Board Collapse、full Stroke PATCH、View delete preserves Entity。

## 23. Scene Lock concurrency

既有 100 并发 Acquire 用例继续在完整服务端套件中 PASS；最终套件 23 个用例全部通过。Server 仍使用原子 acquire、owner 校验和 stale-but-locked 语义，没有新增自动释放。

## 24. XiE Engine 新增通用能力

- `SelectionModel` 支持多种可视对象 SelectionKind 和统一 Items
- `SelectionPopup` 提供过滤、高亮移动、Commit 的通用轻量 Popup 能力
- `WorkspaceCommandHistory` 提供通用 Execute/Undo/Redo 命令历史
- InputKeyCode 和 GLFW/Win32 映射补齐 Workspace 快捷键所需字母键

这些能力没有引入 Git/GitHub/CI/AI 或其他禁止范围。

## 25. Crystal 专有修改

- `PropertyEditor`：属性解析与即时校验
- `WorkspaceEditors`：Query Builder Draft、Schema Editor Draft 和安全编辑辅助
- `CrystalTheme`：Crystal 语义视觉 Token
- `CrystalWorkspace`：Selection、Transform、Clipboard、Context Menu、Panels、Query Draft、Schema Draft、Undo/Redo、Fit/Focus
- `CrystalRenderer`：Layer/Draw/Picker/Query/Schema/Context Menu/Presentation 视觉层
- `CrystalApiClient`、Models、Ink Controller：Board Collapse、Property constraints、Stroke Patch、工具/颜色/宽度状态
- Crystal Server：schema version 4、约束字段、写保护与 Phase 4 tests

## 26. 修改文件列表

主要 Phase 4 文件如下（Crystal 目录本身在当前工作树中为未跟踪目录，未执行 Git 操作）：

- `XiE_Projects/Crystal/Source/Main.cpp`
- `XiE_Projects/Crystal/Source/UI/PropertyEditor.h`
- `XiE_Projects/Crystal/Source/UI/WorkspaceEditors.h`
- `XiE_Projects/Crystal/Source/UI/CrystalTheme.h`
- `XiE_Projects/Crystal/Source/UI/CrystalRenderer.cpp`
- `XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.h`
- `XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.cpp`
- `XiE_Projects/Crystal/Source/Model/CrystalModels.h`
- `XiE_Projects/Crystal/Source/Model/CrystalModels.cpp`
- `XiE_Projects/Crystal/Source/Net/CrystalApiClient.h`
- `XiE_Projects/Crystal/Source/Net/CrystalApiClient.cpp`
- `XiE_Projects/Crystal/Source/Ink/CrystalInkController.h`
- `XiE_Projects/Crystal/Source/Ink/CrystalInkController.cpp`
- `XiE_Projects/Crystal/Tests/CrystalCoreTests.cpp`
- `XiE_Projects/Crystal/CrystalServer/app/database.py`
- `XiE_Projects/Crystal/CrystalServer/app/main.py`
- `XiE_Projects/Crystal/CrystalServer/app/entities.py`
- `XiE_Projects/Crystal/CrystalServer/tests/conftest.py`
- `XiE_Projects/Crystal/CrystalServer/tests/test_phase4_server.py`
- `Engine/Input/KeyCode.h`
- `Engine/Platform/GLFW/GlfwWindowSystem.cpp`
- `Engine/Platform/Win32/Win32WindowSystem.cpp`
- `Engine/UI2D/SelectionModel.h`
- `Engine/UI2D/SelectionPopup.h`
- `Engine/UI2D/WorkspaceCommandHistory.h`

## 27. 已知问题

1. Real Reconnect E2E 和 P_HighSchool 完整人工 UX 尚未执行。
2. 大型 fixture 目前验证数据规模和确定性遍历，未提供真实窗口帧率数据。
3. Layer Create/Rename/Delete/Reorder、Widget 专用 Inspector、Schema CRUD 表单和 Board Rename 仍是轻量第一版/提示路径。
4. Board 拖入已自动 Attach，但拖出自动 Detach 和 Board Highlight 尚未完成。
5. Context Menu 中部分命令目前以提示或现有快捷操作承载，未扩展成完整二级表单。
6. Renderer 仍有少量历史硬编码颜色，Theme Token 迁移尚未覆盖所有绘制点。
7. Workspace Undo/Redo 是客户端本地视觉快照，不覆盖多人外部变化，也不会替代服务器冲突处理。

## 28. Phase 5 建议

1. 先完成双客户端真实断线注入：记录 Disconnected、Reconnecting、Connected、REST reload、pending event 和最终一致性。
2. 用 P_HighSchool 做完整人工 UX checklist，补齐 Layer CRUD、Widget Inspector、Schema CRUD 和 Context Menu 二级操作。
3. 为 Board Attach 增加 hover highlight、拖出 Detach 和可见的 locked feedback。
4. 在 300/100/500/20/20 fixture 上进行真实 Pan/Zoom/HitTest/Inspector profiling，再决定是否需要优化。
5. 继续把 Crystal Renderer 的状态颜色迁移到统一 Token，并补充可视化截图/人工验收记录。

## Acceptance Summary

Dedicated Property Editors: PASS  
Entity Picker: PASS  
Box + Multi Selection: PASS  
Ink Group Transform: PASS  
Layer Panel + Lock: PASS  
Board Drag Resize Attach: PASS  
Dashboard Widget Editing: PASS  
Visual Query Builder: PASS  
Schema Editor: PASS  
Workspace Clipboard: PASS  
Workspace Undo Redo: PASS  
Scene Lock UX: PASS  
Real Reconnect E2E: NOT TESTED  
Large Workspace Interaction: FAIL  
Scene Lock Concurrent Acquire 100 Runs: PASS
