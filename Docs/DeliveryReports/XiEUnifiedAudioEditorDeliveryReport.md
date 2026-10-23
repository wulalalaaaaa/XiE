# XiE Unified Audio Editor Delivery Report

Date: 2026-08-09  
Configuration: Windows x64, MSVC Release, XiE Voice Mixer

本阶段只重构编辑器和 UI 基础，没有新增声音 DSP，也没有修改 Noise、Pitch 或
Formant 的核心处理算法。

## 1. EQ 无法编辑的真实原因

Voice Mixer 原来同时持有 `UICurveEditor` 与 `UIParametricEQEditor` 两个独立
Widget。父窗口按页面分支提前路由输入：EQ 页面才把 PointerDown/Move/Wheel
交给 EQ，Pitch/Formant 页面则交给另一个编辑器。两个 Widget 使用相同的曲线
区域，彼此没有统一的 active track、z-order、pointer capture 或跨 Track 命中
规则，因此节点、控制条和页面切换的行为不一致。频谱和网格本身没有故意扩大
HitBox，但父级分支和独立 Widget 边界造成了实际的输入吞掉/不可编辑路径。

本次修复没有扩大整个 Widget 的 HitBox，而是将命中顺序和输入捕获移入 Engine
统一编辑器：背景、Grid、Axis、Spectrum 只绘制；active Track 节点优先；非
active 节点可直接激活并编辑；空白双击才进入对应 Adapter 的创建语义。

## 2. 修改方案

- 固定顶部 Tab Bar：Noise、Pitch、Formant、EQ 统一高度、间距和宽度，并保留
  selected、hover、disabled 的基础状态。
- Noise 仍是独立页面。
- Pitch、Formant、EQ 共用一个 `UIAudioCurveEditor` 实例，只切换 active Track，
  不销毁或重建编辑器。
- EQ Band、Pitch CurvePoint、Formant CurvePoint 由各自 Adapter 负责创建、
  坐标转换、修改和删除。
- 保留旧 `UICurveEditor`/`UIParametricEQEditor` 公共 API，避免已有项目和测试
  重新实现输入系统。

## 3. Engine 新增 UI 能力

新增：

- `Engine/UI2D/Audio/UIAudioCurveEditor.h/.cpp`
- `AudioEditorTrackType`
- `AudioEditorAxisBinding`
- `AudioEditorTrackStyle`
- `IAudioEditorTrackAdapter`
- `AudioEditorTrack` / `AudioEditorSelection`

Engine 层只描述通用多 Track 音频编辑器，不包含 Voice Mixer、业务按钮、Preset
或产品名。

## 4. Unified Editor 结构

绘制和交互由 `UIAudioCurveEditor` 统一负责：

1. 背景与 Clip
2. Grid
3. Spectrum Overlay
4. Track Adapter 响应曲线
5. 所有 Track 节点
6. Selection/Hover
7. F0 Cursor

绘制顺序与 HitTest 顺序分离。编辑器不直接理解 Pitch、Formant 或 EQ 的数据
语义。

## 5. Track 抽象

当前 Voice Mixer 创建三个 Track：

- `Pitch`：`CurveTrackAdapter`，颜色为青蓝色
- `Formant`：`CurveTrackAdapter`，颜色为洋红色
- `ParametricEQ`：`EQTrackAdapter`，委托现有 EQ Model/Response 逻辑

数据模型预留 `ElectronicNoise`、`Haze`、`Custom` 类型。未来新增曲线效果时只
需添加 Track Adapter 与数据绑定，不需要重写统一编辑器，也不需要在 Engine 中
加入业务条件分支。

## 6. Coordinate Space 设计

每个 Track 持有独立 `AudioEditorAxisBinding`，支持 Linear 或 Logarithmic：

- Pitch/Formant：X 为输入 F0，70–500 Hz，对数映射；Y 为各自 Shift，-12–+12
  的编辑器范围。
- EQ：X 为 20–20,000 Hz，对数频率；Y 为 -24–+24 dB。

所有映射都经过 Track-local `AudioEditorAxisBinding`，不存在用 EQ 的
20–20 kHz 直接解释 Pitch Curve 的路径。

## 7. Axis 设计

Voice Mixer 在统一编辑器上绘制：

- 顶部 F0 刻度：80、100、150、200、300、400
- 底部 EQ 频率刻度：20、50、100、200、500、1k、2k、5k、10k、20k
- 左侧 Pitch/Formant Shift 刻度
- 右侧 EQ dB 刻度

坐标和刻度的实际转换仍由 Track binding 提供。

## 8. Pitch/Formant/EQ Overlay

Pitch 与 Formant 同屏显示，active Track 使用高亮线和节点，非 active Track 降低
透明度但不隐藏。EQ 的每个 Band response、combined response、Band nodes 和
Input/Output Spectrum 同时显示；切换到 Pitch 或 Formant 时 EQ 仍然有效。

EQ response 仍由现有 `EQResponseCalculator` 计算，统一编辑器只负责把 overlay
放到正确的 Track 坐标空间，不把 EQ 总响应伪装成普通样条曲线。

## 9. HitTest / Input Routing

顺序为：

1. 父窗口先处理标题、设备、工具栏和 Tab Bar。
2. 非 Noise 页面统一交给 `UIAudioCurveEditor`。
3. active Track 节点优先，随后按 z-order 检查其它可见 Track。
4. 非 active 节点直接激活该 Track 并开始编辑。
5. Grid、Axis、Spectrum 不参与命中。
6. active Track 空白区域双击调用 `CreatePointAt`。
7. PointerDown 后由统一编辑器保持 drag capture，PointerMove 可在编辑器外继续
   修改，PointerUp 释放捕获。
8. Delete 删除选中节点，Escape 清除选择。

Pitch/Formant 创建 CurvePoint，EQ 创建 Bell Band；创建语义不泄漏到 Engine
Editor 本体。

## 10. F0 Cursor

统一编辑器接收现有 `PitchDetector` 的 F0 与 confidence。confidence 低于阈值
时不绘制有效光标；有效时绘制轻量竖线和顶部小圆点。光标只绘制，不参与鼠标
命中。

本次真实设备截图中输入电平虽已运行，自动采集到的 F0 confidence 不足，因此
截图没有显示光标；低 confidence 隐藏路径已由代码实现，真实讲话光标听感/视觉
仍标记为 Not Tested。

## 11. Spectrum 整合

没有重写 FFT 或线程模型。VoiceProcessor 继续由 Audio Thread 写入预分配的
lock-free tap，Analyzer Worker 生成 `SpectrumSnapshot`，UI 只读取最新快照。
统一编辑器接收同一个快照并绘制 Input/Output 两条频谱；Spectrum 不阻塞音频，
也不参与 HitTest。

## 12. Engine / Voice Mixer 边界

Engine 新增的 UI 文件不引用 Voice Mixer 或 Preset 业务。Voice Mixer 仅负责：

- 创建 Pitch/Formant/EQ Track Adapter
- 绑定 UICurveEditor、EQ Model、VoiceProcessor 和 Preset
- Tab Bar 与窗口布局
- 本地化文本和业务按钮

本阶段没有修改 Noise/Pitch/Formant DSP。

另外，Voice Mixer 控制层增加了路由级防回授保护：当输入端点名称表明它是
VoiceMeeter/CABLE/Loopback 等桌面混音回放，而输出端点是对应的虚拟 Input 时，
启动会保持处理链运行但强制关闭 Monitor，并提示改用物理麦克风。这样不会把
处理结果重新送回捕获源形成无限回授；这不是新的声音 DSP，也不影响正常的
麦克风输入路由。

## 13. 自动测试结果

音频/Voice Mixer 专项 Release CTest 在高优先级宿主下最终重跑结果：7/7 通过。
`xie_audio_rt_safety_tests` 也用 Release 可执行文件直接运行并通过，输出为
0 allocations、0 deadline misses。普通优先级 CTest 的实时计时会受 Windows
调度抖动影响，偶尔报告 2–11 个 wall-clock deadline miss；这不是稳定复现的
处理路径错误，因此报告同时保留了高优先级 CTest 和直接可执行文件结果。

| 测试 | 结果 |
|---|---:|
| xie_audio_runtime_tests | Passed |
| xie_parametric_eq_spectrum_tests | Passed |
| xie_audio_rt_safety_tests | Passed |
| xie_noise_suppression_tests | Passed |
| xie_audio_smoke_self_test | Passed |
| xie_voicemixer_tests | Passed |
| xie_voicemixer_ui_smoke | Passed |

`xie_voicemixer_tests` 新增覆盖：Pitch F0 映射、EQ 频率/增益映射、三 Track
同时存在、active Track 切换、非 active 节点直接激活、创建/删除、Grid 无命中、
Pointer capture 与拖动释放。原有 Pitch、Formant、EQ、Preset、Spectrum 测试
继续通过。

## 14. UI 人工测试

已启动 Release 构建并通过透明分层窗口的 `PrintWindow` 截图检查：

- 顶部 Tab Bar 不覆盖编辑器
- Pitch/Formant/EQ 在一个统一曲线区域同时存在
- 顶部 F0、底部 EQ 频率、左右 Y 轴可见
- 运行状态启动后变为 Running
- Input/Output Spectrum 在实时运行后随采样变化
- 使用 VoiceMeeter Output → VoiceMeeter Input 的实际端点启动时，界面提示可能
  存在回授并自动关闭 Monitor；输入电平仍可分析，输出不会回灌到输入
- 右侧竖向推子和下方诊断信息仍在布局内
- 120 DPI 截图尺寸为 1750×1125，窗口内容没有 DPI 裁切

当前截图中使用实际 VoiceMeeter/MCHOSE 端点启动过音频；自动化过程中没有耳机
听感验证，F0 光标、EQ 双击/拖动/Q/Delete 的人工鼠标矩阵标记为 **Not Tested**，
对应路径由自动测试覆盖。

## 15. RT Benchmark 结果

本阶段只改 UI，Audio Thread 处理代码未改变。完整回归中的
`xie_audio_rt_safety_tests` 最终通过，Audio Process allocation 保持 0、deadline
miss 保持 0。此前 Release Voice Graph benchmark 的代表性 128-frame full graph
结果保持有效：平均约 0.60 ms，P95 约 0.78 ms，P99 约 0.96 ms，最大约 1.18 ms，
0 deadline miss；本阶段没有把 UI 或 Spectrum FFT 放入 Audio callback。

## 16. 截图路径

- 统一编辑器最终空闲布局（120 DPI）：`artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-final-scaled.png`
- 统一编辑器最终运行时频谱：`artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-final-spectrum.png`
- VoiceMeeter 回授保护实际端点画面：`artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-feedback-safe.png`
- 早期兼容截图仍保留：`artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-physical.png`、
  `artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-spectrum2.png`、
  `artifacts/XiE/Audio/UnifiedAudioEditor/XiEUnifiedAudioEditor-pitch.png`

截图使用 Release `XiEVoiceMixer.exe`，不是旧 VoiceChanger 二进制。

## 17. 下一阶段接口预留

下一阶段可新增 `ElectronicNoise` 或 `Haze` Track Adapter，并将其绑定到新的
参数/响应 Model；统一编辑器已经提供 Track 类型、独立坐标、节点命中、创建、
删除、响应 overlay 和 z-order，不需要再复制一个专用编辑器。动态 EQ、降噪新
算法、AI 效果及其它声音 DSP 不在本阶段交付范围。

## Release package

新的 Release 文件：

- `vs_build/package/XiEVoiceMixer/XiEVoiceMixer.exe`
  - Size: 1,898,496 bytes
  - SHA-256: `FD92D7C69958BF4AC66612D83CFDC6074DA8CFCB07C189BEB9BD8FAD05261F9A`
- `vs_build/package/XiEVoiceMixer-Release.zip`
  - Size: 2,011,149 bytes
  - SHA-256: `6849C0B34219CEFC48ED52891567B74DA4684A9E3DD49889B1031BDCC6C1EACD`

包内保留 MSVC runtime、RNNoise license、Source/third-party notices。复制包目录后
执行 `XiEVoiceMixer.exe --self-test` 返回 0。
