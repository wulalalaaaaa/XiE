# XiE / Crystal Frame Scheduling、Present Profiling 与 Renderer Cache 修复报告

日期：2026-08-16  
范围：XiE Engine、GLFW/Win32 平台层、Crystal Main Loop、CrystalRenderer。  
原则：Crystal 复用 XiE `FrameScheduler`；没有新增 `CrystalFrameLimiter`，也没有修改 Crystal Server、Scene Lock 业务或无关 Audio/Voice 项目。

## 1. 最终根因

旧 Crystal 主循环在每轮 `PollEvents → PumpRealtimeEvents → Render → glfwSwapBuffers`。VSync 只会阻塞交换时机，并不会决定这一轮是否需要生成图形，因此静止窗口仍然持续做 CPU 遍历、文本/几何构建、上传和 Present。Renderer 同时把大量 workspace 内容重新组织为顶点，导致静态场景也持续占用单线程和 GPU。

## 2. XiE 原 FrameScheduler 状态

XiE 原来已经有 `Engine::FrameScheduler` 和 `WindowContext`，但没有 Crystal 需要的请求原因、按窗口 dirty 请求合并、最小化/可见性策略及活动帧间隔。此次在原类上扩展了 `FrameReason`、`RequestFrame`、`AcknowledgeFrame`、`ForgetWindow` 和 paced `Evaluate`，没有复制第二套调度器。

## 3. Crystal 为什么持续 Render

旧循环没有把“有事件”和“需要绘制”分开，也没有在静态状态进入事件等待。即使 `glfwSwapInterval(1)` 让交换受显示器节奏限制，RenderBuild 仍然会被反复执行；这解释了旧版静态单核心 17%～30% 和 GPU 3D 9%～13% 的观测。

## 4. Present profiling

Main Loop 现在分别记录：`EventPump`、`RealtimePump`、`Update`、`RenderBuild`、`GpuSubmit`、`Present`、`FrameTotal`、`IdleWait`。`CrystalProfiler` 的 CPU `Present` scope 包住 `Engine::IRenderSurface::Present()`，因此不再把 swap/present 排除在统计之外。统计输出到 `CrystalFrameStats.csv`，详细 scope 追加到 `CrystalProfile.csv`。

Debug OpenGL timer query 使用 `GL_TIME_ELAPSED`，只在 `GL_QUERY_RESULT_AVAILABLE` 时非阻塞读取；没有使用 `glFinish()`、`glFlush()` 或人为等待 GPU。最终 Debug fixture 实跑得到 `GpuDrawTime=0.111296 ms`、`GpuTimerSamples=1`。Release 构建不启用该 Debug-only query。

## 5. VSync 实际行为

GLFW 的 `glfwSwapInterval(1)` 保留，继续承担防 tearing / present policy。`FrameScheduler` 独立决定是否存在这一帧：静止状态不 Render/Present；交互状态以 60 FPS 为目标；可见但失焦的活动动画使用 15 FPS 目标。VSync 不再充当 FrameScheduler，也没有通过关闭 VSync 规避问题。

## 6. FrameScheduler 修复

`FrameReason` 包含 `Input`、`Animation`、`Realtime`、`WindowExpose`、`Resize`、`Camera`、`WorkspaceChange`、`Explicit`、`WindowRestore`。请求按窗口合并并按优先级保留，事件数量不会直接变成渲染数量。`Active` 使用 `steady_clock` 间隔和下一次 deadline，`Static` 没有工作时直接返回不渲染，`VisibleIdle` 为 15 FPS，`Hidden`/最小化只等待事件。

## 7. Wait / Wake 设计

Engine 的 `IApplicationHost` 增加 `WakeEventLoop()`。GLFW host 使用 `glfwWaitEventsTimeout()` 和 `glfwPostEmptyEvent()`；Win32 host 使用线程消息队列和 `PostThreadMessageW()` 唤醒。Crystal 只调用 host 抽象，不直接用 GLFW API 做调度。`MainThreadEventQueue` 在入队后复制 callback、解锁，再触发 callback，避免持锁唤醒造成竞态。

## 8. OnDemand Rendering

静态窗口没有 dirty/request/animation/command 时，主线程进入可唤醒的 event wait，不执行 Render 或 Present。最终 Debug 离线空闲实跑 5 秒：`FramesRendered=1`、`PresentCount=1`，只有启动/窗口事件；不是持续 300 或 600 次 Present。

## 9. Minimized policy

`WindowSystem` 暴露 `IsVisible`、`IsMinimized`、`IsFocused`、`GetFramebufferSize`。最小化时 `Evaluate` 不允许 Render/Present，仍继续处理网络和模型事件；恢复时请求 `WindowRestore` 并做一次完整 redraw。实跑统计包含最小化前的 1 次初始 Present，但最小化后的 8 秒窗口区间有 89 次 skipped、0 次新增 Present，满足“最小化后零 Present”。

## 10. Background policy

失焦可见窗口不会继续按 Active 60 FPS 跑；失焦的 Active policy 会降为 `VisibleIdle`，无动画时回到静态按需模式，有动画时最高 15 FPS。本轮没有在 Upupoo/虚拟显示设备开关组合下完成独立的失焦功耗 A/B，见剩余风险。

## 11. Active frame pacing

交互由 `steady_clock + WaitForEventsTimeout()` pacing，目标 60 FPS，没有使用 `Sleep(16)`，也没有强行改成 30 FPS。100 次高频 `Camera` request 在 scheduler 测试中只产生有限的 paced frames；真实 Debug 中 180 次中键拖动事件在 2.864 秒输入窗口内得到 23 次 Render、23 次 skipped，实际 Present 约 8.0/s（事件注入器本身不是 1000 Hz 显示器基准）。

## 12. Render Request / Dirty 传播

Crystal 在 workspace/realtime、resize/DPI/expose/restore、camera pan/zoom、输入、选择/拖拽/绘制等内容发生变化时调用 `RequestFrame(reason)`；只移动相机使用 `Camera`，不会直接使整个 geometry cache 失效。实时队列通过 `SetRealtimeWakeCallback()` 唤醒 host，主线程 `PumpRealtimeEvents()` 后以 `Realtime` 请求一帧。Render 完成后 `AcknowledgeFrame()` 清掉该窗口的 pending request。

## 13. Renderer cache

CrystalRenderer 增加两层缓存：同一 camera/framebuffer 下的整帧缓存，以及 world object geometry cache。后者按对象 key 覆盖 View、Board、Widget、Relationship、Ink，fingerprint 纳入对象 ID、revision/位置/尺寸、可见性、图层、选择/hover/lock、关系端点、Ink 点列和相关样式字段。相同 fingerprint 直接复用 world vertices，改变后只重建该对象。

最终 Debug fixture 实跑的 3 个 Render 帧统计为 `ObjectCacheHits=8`、`ObjectCacheMisses=4`、`GeometryBuilds=3`，没有按对象数量逐帧重建。

## 14. Camera transform

world geometry 在 camera-independent 坐标中缓存，Flush 阶段统一应用 camera center/zoom 到屏幕顶点。Pan/Zoom 只重新组合当前 viewport 并复用可见对象 cache；相机移动不会调用全局 `InvalidateGeometryCache()`，新进入 viewport 的对象才可能首次建立自己的 entry。实际中键拖动样本出现大量 object cache hit，且 Camera request 被 scheduler pacing 合并。

## 15. Text cache

当前 Crystal 使用固定 5×7 glyph 路径，`DrawText` 将相同字符串的 glyph cell layout 缓存在 `m_TextLayoutCache`，缩放在提交时处理，不为每次绘制重新 layout/glyph geometry。最终 Debug fixture 统计 `TextLayoutBuilds=35`，而不是每次文本调用都重新生成。

XiE 的通用 `BasicTextLayoutService` 另有 font/maxWidth/fontSize/DPI bitmap cache；Crystal 当前固定 glyph renderer 没有强行接入那套 bitmap backend，这是一个已记录的架构边界和后续风险，不影响本轮固定 glyph path 的字符串布局复用。

## 16. Ink cache

完成的 Stroke 以对象 fingerprint 和 world vertices 缓存；点列、宽度、工具、形状、锚定对象、图层或选择状态改变时失效。绘制中的 Stroke 仍允许随输入更新，完成后静态帧不会重复 tessellate 同一 Stroke。

## 17. Relationship cache

Relationship 以 source/target 端点、类型、可见性、选择状态及端点坐标组成 fingerprint。端点移动或 edge/style 状态改变才重算；静态帧复用缓存，并在 AABB/segment bounds 不可见时跳过。

## 18. VBO 修改

VBO 使用 capacity-based dynamic storage：容量不足时才 `glBufferData(..., nullptr, GL_DYNAMIC_DRAW)` 几何增长，普通帧使用 `glBufferSubData()`。顶点、转换顶点和 world cache vector 预留并清空复用，避免每帧重新分配 buffer storage。源码扫描显示正常 Render loop 没有每帧 `glBufferData` 重分配。

## 19. Visible culling

继续使用简单 AABB/segment-vs-viewport culling，先跳过 viewport 外的 View、Board、Widget、Ink bounds 和 Relationship，再进行文本、几何和提交。最终 fixture 仍加载 300/100/500/20/20 对象，但实际一帧提交 `Vertices=33732`，验证了 viewport culling 路径在运行。

## 20. CPU/GPU sync 检查

对 Crystal、GLFW 和 Renderer2D 进行了 `glFinish`、`glFlush`、buffer map、`glReadPixels` 与同步路径扫描：未发现正常 Render loop 中的这些强制同步。剩余的 `glGetShaderiv`、`glGetProgramiv`、`glGetUniformLocation` 属于初始化/绑定 setup；Debug timer 只读取 query availability，不调用 `glFinish`。

## 21. Debug Before/After

旧版基线是仍在运行的旧 `crystal_client.exe`（PID 78496）10 秒 `TotalProcessorTime` 增量 2.5312 秒，即约 25.31% 单逻辑核等效 CPU；该进程未被本轮终止或替换。最终 Debug 变体为 `artifacts/crystal_debug/crystal_client_perf.exe`，离线空闲 5 秒 CPU 增量 0.109375 秒，即约 2.19% 单逻辑核等效 CPU，按这两次采样约降低 91.4%。此前另一组 10 秒 invalid-server warm idle 采样为 0.469%，作为补充而非替代基线。

Debug fixture 的最终 5 秒采样：3 Render、3 Present、25 skipped、7 requests、2.5% 单逻辑核等效 CPU、`RenderBuildTime=34.6663 ms`（该字段为本次采样 scope 累计/统计格式，不等同单帧 wall-clock）、`PresentTime=0.0462 ms`、GPU query 0.111296 ms/1 sample。

## 22. Release Before/After

最终 Release 默认目标 `vs_build/XiE_Projects/Crystal/Release/crystal_client.exe` 成功构建。离线空闲 5 秒 CPU 增量 0.09375 秒，即约 1.875% 单逻辑核等效 CPU；统计为 1 Render、1 Present、`GeometryBuilds=1`、`TextLayoutBuilds=18`、`RenderBuildTime=0.7708 ms`、`PresentTime=0.0542 ms`。Release 不输出 Debug GPU query sample。

Release 另有实际离线交互采样：120 次中键移动产生 112 Render/Present、68 skipped、114 requests，其中 109 个为 Camera request；输入持续时间未记录，因此不把该组换算成 FPS。

## 23. Idle Before/After

旧版静态循环持续 Poll/Render/Swap，基线 CPU 为约 25.31% 单核。修复后 Debug/Release 都只在启动/必要窗口事件完成首帧，随后进入 `IdleWait`；最终两组 5 秒离线采样均为 1 次 Present，证明静态状态不再持续 present。

## 24. Interactive Before/After

修复后输入被转为 dirty reason 而不是立即绘制命令，scheduler 将高频 MouseMove/Camera request 合并并按 60 FPS deadline pacing。Debug 真实拖动样本为 23 Render、23 skipped；Release 离线样本为 112 Render、68 skipped。没有在 120/144/240 Hz 多显示器上做精确 present-FPS A/B，因此这些数字用于验证合并/跳帧，不宣称硬件刷新率结论。

## 25. Minimized Before/After

旧路径没有统一的 minimized short-circuit。修复后最小化区间不调用 Render/Present，网络/事件泵仍保持工作；实测总统计虽含最小化前 1 次 Present，但最小化后 89 次 scheduler skip 均无新增 Present。

## 26. 60Hz / High Refresh 对比

代码层面的 Active target 固定为 60 FPS，独立于 `glfwSwapInterval(1)`，所以高刷新显示器不会自动把 Crystal 推到 144/240 FPS。实际 60 Hz 与高刷新显示器的成对硬件 A/B 本轮未完成：`NOT TESTED`。

## 27. Upupoo / Virtual Display A/B

没有可控地切换 Upupoo、DWM 和虚拟显示设备并重复同一拖动/静止采样，因此 A/B 结论为 `NOT TESTED`，没有把用户提供的旧 GPU 观测冒充本轮实测。

## 28. Large Workspace 数据

`--fixture` 实际加载：300 Entity Views、100 Relationships、500 Ink、20 Boards、20 Widgets。最终 Debug 5 秒采样为 3 Render、25 skipped、3 Present、33732 vertices、809568 uploaded bytes、3 geometry builds、8 object-cache hits、4 misses、35 text-layout builds，并记录 `WorkspaceViews=300`、`Relationships=100`、`InkStrokes=500`、`Boards=20`、`Widgets=20`。本轮实际注入了 Pan/Camera 事件；Draw、Selection、Box Selection、Inspector、Layer Toggle 的完整交互清单没有全部手工执行。

## 29. XiE tests

`xie_runtime_host_tests` 本轮无法链接，因为工作区已有的 `Engine/Core/Application.h/.cpp`、`Engine/Renderer/Renderer.h/.cpp` 冲突标记（`<<`、`==`、`>>`）以及 `TextureAsset.h`、`Camera2D.h`、`Assets/AssetManager.h` 缺失导致 `xie_engine` 先行编译失败。这些错误与本次 Crystal 调度/Renderer 改动无关，未扩大范围修复。新增的 RuntimeHost FrameScheduler 测试源已保留，但该 target 状态是 `NOT TESTED / BLOCKED BY PRE-EXISTING ENGINE BUILD`。

## 30. Crystal tests

`crystal_core_tests` Debug 和 Release 均构建并通过 CTest（100%）。覆盖包括 MainThreadEventQueue FIFO + 32 次 wake callback、RequestFrame/reason、minimized、active animation、100 次 request coalescing 和高频 Camera request 的 60 FPS pacing。Crystal Debug alternate client 与 Release client 均成功链接并实际启动。

## 31. 修改文件列表

本轮任务相关文件如下；工作区其余已有 dirty/untracked 内容未清理：

- `Engine/Application/ApplicationRuntime.cpp`
- `Engine/Application/FrameScheduler.h/.cpp`
- `Engine/Application/WindowContext.h`
- `Engine/Core/MainThreadEventQueue.h`
- `Engine/Platform/IApplicationHost.h`、`IWindowSystem.h`
- `Engine/Platform/GLFW/GlfwApplicationHost.h/.cpp`、`GlfwWindowSystem.h/.cpp`
- `Engine/Platform/Win32/Win32ApplicationHost.h/.cpp`、`Win32WindowSystem.h/.cpp`
- `XiE_Projects/Crystal/CMakeLists.txt`
- `XiE_Projects/Crystal/Source/Main.cpp`
- `XiE_Projects/Crystal/Source/UI/CrystalProfiler.h`
- `XiE_Projects/Crystal/Source/UI/CrystalRenderer.h/.cpp`
- `XiE_Projects/Crystal/Source/Net/CrystalRealtimeClient.h`
- `XiE_Projects/Crystal/Source/Workspace/CrystalWorkspace.h/.cpp`
- `XiE_Projects/Crystal/Tests/CrystalCoreTests.cpp`
- `XiE_Projects/RuntimeHostTests/Source/RuntimeHostTests.cpp`

## 32. 剩余风险

1. 没有完成 60 Hz/高刷新率、Upupoo/虚拟显示设备、DWM 拖动的受控 A/B。
2. 没有完成大型 Workspace 的 Draw、Selection、Box Selection、Inspector、Layer Toggle 全量手工回归。
3. Crystal 当前使用固定 5×7 glyph cache，尚未把 XiE 通用 `BasicTextLayoutService` 的 font/maxWidth/fontSize/DPI bitmap cache 接入 Crystal 的自定义 GL glyph path。
4. Debug timer query 是异步、低频样本，不能替代 GPUView/厂商 GPU utilization 的长期功耗测量；本轮没有同环境旧版 GPU 3D utilization 对照。
5. object fingerprint 是 Crystal 当前模型字段的稳定哈希，主题系统若扩展出未纳入 fingerprint 的字段，需要同步补充失效条件。
6. `xie_runtime_host_tests` 仍受既有 xie_engine 编译问题阻塞；需要单独恢复 Engine 工作区后再做完整 XiE 回归。

## 最终验收字段

Idle On-Demand Rendering: **PASS**（最终 Debug/Release 空闲 5 秒各 1 次 Present）

Minimized Zero Present: **PASS**（最小化后零；总统计含最小化前 1 次初始 Present）

Realtime Wake From Idle: **PASS**（MainThreadEventQueue wake callback 测试通过，真实运行记录 Realtime request）

Interactive Frame Cap: **PASS**（scheduler 100 次 Camera request 合并；真实输入出现 skipped frames，代码目标 60 FPS）

Static Geometry Cache: **PASS**（静态实跑首帧后无持续 geometry build；fixture 有 object cache hits）

Camera Pan Without Full Geometry Rebuild: **PASS**（world cache + camera transform；Camera 不触发全局 geometry invalidation）

Text Layout Cache: **PASS**（当前固定 5×7 glyph path；通用 XiE bitmap service 接入仍是剩余风险）

Large Workspace Interaction: **NOT TESTED**（fixture 与 Pan 已实跑，但完整交互清单未全部执行）

Crystal Idle CPU Reduction: **实际旧版 25.31% → 最终 Debug 2.19% 单逻辑核等效 CPU（5 秒采样，约降低 91.4%）；Release 实测 1.875%**

Crystal Idle GPU Reduction: **实际旧观测 9%～13% → 未完成同环境 GPU 3D utilization A/B（NOT TESTED）**

DWM Drag A/B: **NOT TESTED**

