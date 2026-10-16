# XiE Parametric EQ 与实时频谱交付报告

交付日期：2026-08-02  
交付应用：**XiE Voice Mixer**  
构建配置：Visual Studio / Release / Windows x64

## 1. 引擎新增模块

- `Engine/Audio/DSP/EQ/ParametricEQ.*`：通用参数均衡器数据模型、Snapshot、Biquad、处理器与响应计算。
- `Engine/Audio/Analysis/SpectrumAnalyzer.*`：输入/输出实时频谱采集、分析线程与最新帧 Snapshot。
- `Engine/UI2D/Audio/UIParametricEQEditor.*`：通用 EQ 编辑器，不依赖 Voice Mixer 业务。
- `VoiceProcessor` 只组合通用模块；未重新实现 AudioBuffer、AudioGraph、Snapshot 或输入系统。
- 本轮未开发降噪、动态 EQ、De-esser、多段压缩、VST 宿主或 Voicemeeter Remote API。

## 2. EQ Band 数据模型

`ParametricEQBand` 使用稳定 `EQBandId`，持久身份不依赖数组索引。支持：

- Bell
- Low Shelf
- High Shelf
- Low Cut
- High Cut
- Notch

限制集中定义为：最多 16 段、20 Hz–20 kHz、-24 dB–+24 dB、Q 0.1–24、Slope 6–24 dB/oct。`ParametricEQModel` 统一负责增加、修改、删除、查找、重置、ID 分配和参数 Clamp。

## 3. Biquad / 滤波算法

第一版采用经过广泛验证的 RBJ Audio EQ Cookbook 形式计算数字 Biquad 系数。每个频段独立处理，各段串联形成总响应，未使用普通样条曲线伪造 EQ 响应。支持 44.1/48 kHz、Mono/Stereo，并预留最多 8 声道的固定状态空间。

Low/High Cut 当前由单级二阶 Biquad 实现，实际渐近斜率固定为 12 dB/oct；`slopeDbPerOctave` 已进入模型、Preset 和 UI，但 6/18/24 dB/oct 的多级滤波实现留作后续。这一限制没有在 UI 或报告中伪装为已完成。

## 4. Snapshot 发布机制

普通线程校验 Model 并构建不可变 `ParametricEQSnapshot`，其中包含已计算系数、稳定 ID、启用状态和 Revision。Audio Thread 只通过原子指针读取当前 Snapshot。

沿用 XiE 现有“保留 shared ownership + 原子发布 raw pointer”的生命周期策略；删除频段不会释放 Audio Thread 正在读取的对象。`Process` 热路径无 mutex、JSON、日志格式化和 UI 访问。

## 5. 参数平滑机制

Snapshot 切换时按约 2 ms 对 Biquad 系数做短时间线性插值。复杂系数只在普通线程计算，不按 sample 重算。每个声道/频段的滤波状态使用 Prepare 阶段建立的固定数组，Process 中不进行堆分配。

## 6. EQ 总响应计算

`EQResponseCalculator` 在普通线程以对数频率点计算每一段的复频率响应，并把各段 dB 响应相加得到总响应。UI 默认使用 1024 个点；仅在 Band 参数或采样率变化时重算，Spectrum Snapshot 变化不会触发 EQ 响应重算。

## 7. Spectrum Analyzer 线程模型

线程路径为：

```text
Audio Thread
  -> 预分配 SPSC Sample Ring（输入/输出各一份）
Analyzer Worker
  -> Hann Window -> FFT -> Magnitude/dB -> Attack/Release
  -> 原子发布最新 SpectrumSnapshot
UI Thread
  -> 只读取最新 Snapshot
```

默认 FFT 2048、48 kHz 下约 30 Hz 更新、显示 20 Hz–20 kHz / -100 dB–0 dB。支持 4096 FFT。Worker 落后时只保留最新可发布结果并累计 dropped windows，不要求 UI 消费历史帧。

## 8. FFT 实现与许可证

工程中未发现可复用 FFT 依赖。为避免引入大型依赖，本轮实现了紧凑的 radix-2 iterative FFT，仅在 Analyzer Worker 上运行，支持当前 2048/4096 power-of-two 配置。

该实现为本项目新增代码，不包含第三方源码，因此没有新增第三方许可证义务。自动测试验证 Hann 窗、bin 映射、100/440/1000 Hz、多正弦、静音、白噪声、dB 转换与 Attack/Release。

## 9. Input / Output Analysis Tap

最终 VoiceProcessor 链为：

```text
Input
-> Input Spectrum Tap
-> DC Blocker
-> Pitch Detector
-> Pitch Shift
-> Formant Shift
-> Parametric EQ
-> Limiter
-> Output Spectrum Tap
-> Output
```

输入与输出使用独立 ring，Snapshot 同时发布 `frequencyBins`、`inputMagnitudeDb`、`outputMagnitudeDb`、timestamp、revision 和 peak dB。架构允许以后移动或增加 Pre/Post EQ tap，本轮只维护一套 EQ UI。

## 10. UIParametricEQEditor

编辑器位于 Engine UI 层，而非 Voice Mixer 项目层。功能包括：

- 20 Hz–20 kHz 对数坐标、-24 dB–+24 dB 坐标、0 dB 基线和频率/dB 网格。
- 输入/输出频谱、每段局部响应、总响应、所有 Band 节点。
- 双击空白新增 Bell；点击选择；拖动频率/增益；滚轮调 Q。
- 右键循环 Band Type；Delete 删除；双击节点启用/禁用。
- Footer 精确显示 Enabled、Type、Frequency、Gain、Q、Slope、Delete。
- 对 Cut/Notch 隐藏无意义 Gain，滚轮可对当前 Footer 参数精调。

16 Band、1024 响应点、17,445 条 Draw Command 的 100 帧 Release 测量平均 **0.205 ms/帧**。频谱顶点只在 Spectrum Revision 变化时更新。

## 11. Voice Mixer 三页面切换

应用新增明确 `VoiceEditorPage { Pitch, Formant, EQ }` 状态：三个页面互斥可见，切换只改变可见性，不销毁编辑器。隐藏 EQ 页面时 EQ DSP 与 Analyzer 继续运行。

上次页面保存到 `%LOCALAPPDATA%\XiE\VoiceMixer\LastEditorPage.txt`，重启恢复。EQ 具有独立 Enabled/Bypass 和 Reset EQ；Reset EQ 清空所有 Band，恢复 0 dB。

## 12. Preset 格式与迁移

Preset Schema 升级为 V2：

```json
{
  "pitchCurve": {},
  "formantCurve": {},
  "eq": {
    "enabled": true,
    "bands": []
  }
}
```

EQ Band 保存稳定 ID、Enabled、Type、Frequency、Gain、Q、Slope。缺少 `eq` 的旧 Preset 自动迁移为 Enabled + 空 Bands 的 Neutral EQ。继续使用原子保存、稳定字段顺序，并保留顶层及 EQ 对象内未知字段。单个 Band 对象内的未知字段目前不做逐 Band 保留，列入已知限制。

应用名称、目标、源码目录和 UI 已改为 **XiE Voice Mixer / XiEVoiceMixer.exe / `XiE_Projects/VoiceMixer`**。首次运行会按需从 `%LOCALAPPDATA%\XiE\VoiceChanger` 迁移旧 Preset、语言和页面状态，因此代码中只为兼容迁移保留旧名称。

## 13. CTest 结果

最终相关测试：**4/4 通过，0 failed**。

```text
xie_parametric_eq_spectrum_tests  Passed
xie_audio_rt_safety_tests         Passed
xie_voicemixer_tests              Passed
xie_voicemixer_ui_smoke           Passed
```

覆盖空/禁用 EQ、所有六类滤波器、多 Band、Mono/Stereo、44.1/48 kHz、Reset、1000 次 Snapshot、BandId、Clamp、坐标双向映射、页面切换、旧 Preset 迁移、未知字段、频谱测试信号及输入/输出 Snapshot。

## 14. RT Stress 结果

Release 压力测试结果：

```text
Audio Process allocations: 0
concurrent Curve/Parameter/EQ publications: 1000
max process: 0.1252 ms
deadline misses: 0
```

另外完成 1000 次 Pitch/Formant/EQ 页面切换，EQ 页面隐藏后 Model 与 DSP Enabled 状态仍保留。测试未发现 data race、use-after-free 或流被页面切换停止。

## 15. Benchmark 结果

最终独立 Release 复测，48 kHz / 128 frames / Stereo：

| 场景 | Avg ms | P95 ms | P99 ms | Max ms | Deadline % | RT alloc | Miss |
|---|---:|---:|---:|---:|---:|---:|---:|
| Voice，无 EQ/Analyzer | 0.5338 | 0.5612 | 0.6372 | 0.6692 | 20.0191 | 0 | 0 |
| EQ 1 Band | 0.5334 | 0.5580 | 0.5969 | 0.6200 | 20.0028 | 0 | 0 |
| EQ 8 Bands | 0.5366 | 0.5515 | 0.5708 | 0.6963 | 20.1231 | 0 | 0 |
| EQ 16 Bands | 0.5464 | 0.5747 | 0.6134 | 0.6507 | 20.4883 | 0 | 0 |
| Input/Output Analyzer taps | 0.5374 | 0.5686 | 0.6423 | 1.0671 | 20.1509 | 0 | 0 |

最终复测所有表列场景 0 allocation、0 deadline miss。前一轮 Mono/16 Band 曾记录一次 4.38 ms 的系统调度尖峰并计 1 miss；紧接的独立完整复测 Mono/16 Band 为 max 0.7552 ms、0 miss。专门 RT Stress 同样为 0 miss，因此保留该偶发观测，不将其隐藏。

## 16. 实机 Voicemeeter 结果

已在本机完成以下实际端点链路：

```text
Mic (MCHOSE V9 PRO)
-> VoiceProcessor
-> Parametric EQ
-> VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)
```

应用进入 Running 并持续 8 秒后停止，端点正常释放。现场诊断：总延迟 49.3 ms、callback 0.07 ms / max 0.10 ms、deadline 0.4%、XRUN/UNDER 0/0、RT allocation 0、FFT 2048 / 30 Hz、Analyzer Worker 0.11 ms、Spectrum revision 236 / dropped 0、EQ process 0.002 ms。

测试截图：`artifacts/xie-voice-mixer-live-spectrum.png`。

## 17. 人工耳机听感结果

**Not Tested**。

已验证实际 WASAPI/Voicemeeter 端点链、运行状态、EQ 参数发布和频谱 Snapshot；测试截图时麦克风没有讲话，因此未把静音画面误报为“人声频谱随讲话变化”，也没有仅凭测试信号声称人声听感通过。仍需用户佩戴耳机完成音质、click/pop 与变声听感验收。

## 18. 已知限制

- Cut 的实际 DSP 斜率当前固定 12 dB/oct；6/18/24 多级实现未完成。
- FFT 仅接受 power-of-two，当前正式配置为 2048/4096。
- Peak hold 是需求中的可选项，本版未实现；Attack/Release 已实现。
- Footer 精调使用滚轮，没有另做文本输入框。
- Preset 保留顶层/EQ 根对象未知字段，但不保留每个 Band 对象内部的未知字段。
- 人工耳机听感和讲话时输入/输出频谱视觉区分尚未人工验收。
- Snapshot 所有权沿用现有 retained-snapshot 策略；频繁发布对象随 Processor 生命周期统一回收，而不是在 Audio Thread 上回收。

## 19. 下一阶段建议

1. 先做人工耳机与讲话频谱验收，记录真实听感、click/pop 和输入/输出差异。
2. 用级联 Biquad 正式实现 Cut 6/12/18/24 dB/oct，并增加对应响应测试。
3. 增加 Band Footer 的键盘精确输入与可选 Peak Hold。
4. 若未来需要，再单独设计 Pre-EQ/Post-EQ；本轮不扩张为两套 UI。
5. 降噪、动态 EQ 等继续作为独立阶段，不与本轮静态 EQ 混合。

