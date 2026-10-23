# XiE Voice Mixer 第二阶段交付报告：Soft Haze + Spatial Haze

日期：2026-08-09  
项目：XiE Voice Mixer；引擎扩展：XiE Audio Runtime

## 1. 引擎新增模块

新增通用、与 Voice Mixer 业务无关的 Engine DSP 原语：

- `Engine/Audio/DSP/Delay/DelayLine.{h,cpp}`：预分配环形延迟线。
- `Engine/Audio/DSP/Diffusion/AllPassDiffuser.{h,cpp}`：带反馈和阻尼的短全通扩散器。
- `Engine/Audio/DSP/Diffusion/DiffusionNetwork.{h,cpp}`：固定四级扩散网络。
- `Engine/Audio/DSP/Filter/DampingFilter.{h,cpp}`：轻量一阶阻尼低通。
- `Engine/Audio/DSP/Haze/SoftHaze.{h,cpp}`：短微扩散柔化处理器。
- `Engine/Audio/DSP/Haze/SpatialHaze.{h,cpp}`：短早期反射、扩散和立体宽度处理器。

`xie_audio_dsp` 统一编译这些模块；DSP 不依赖 UI、Voice Mixer 或 Renderer/Runtime2D。

## 2. EQ/曲线阶段与本阶段关系

Parametric EQ 和 Pitch/Formant 曲线继续使用上一阶段已经完成的实现。本阶段没有把 Haze 错误地做成曲线，也没有修改 EQ 总响应模型。

## 3. Haze 数据模型

`SoftHazeConfig`：`enabled`、`amount`、`blur`、`damping`、`wet`。  
`SpatialHazeConfig`：`enabled`、`amount`、`size`、`diffusion`、`damping`、`width`、`wet`。

所有浮点参数集中 Clamp 到 `[0, 1]`，非法 NaN/Inf 回退到安全默认值。两个处理器均提供带稳定 `revision` 的不可变 Snapshot 和独立 Metrics。

## 4. Biquad/滤波算法说明

本阶段 Haze 没有引入 Biquad；短空间效果使用预分配 DelayLine、反馈 All-Pass、四级 DiffusionNetwork 和一阶 DampingFilter。这样可以保持低成本、短尾、无明显回声的设计目标。Parametric EQ 的 Biquad/IIR 实现保持不变。

## 5. Soft Haze 算法

Soft Haze 使用 1--10 ms 可调短延迟、低反馈扩散和阻尼滤波，并以 dry/wet 并行混合输出。它不会生成长混响尾、明显 echo 或 chorus。默认 `enabled=false`，默认 wet 较低。

## 6. Spatial Haze 算法

Spatial Haze 使用 7/11/17/23 ms 基础早期反射间隔，按 Size 缩放，经过 Diffusion 和 Damping 控制，再以 Mid/Side Width 调整立体宽度。wet 通路保持较低，默认关闭；不会变成长 reverb、ping-pong delay 或旋转声场。Metrics 中的 `effectiveTailMilliseconds` 是扩散跨度估计值，不是长混响尾的测量值。

## 7. Snapshot 发布和参数平滑

主线程创建并保留 `SoftHazeSnapshot` / `SpatialHazeSnapshot`，通过原子裸指针发布；Audio Thread 只在 block 边界采纳新指针。所有 Amount、Blur、Size、Diffusion、Damping、Width、Wet 和启停状态都经过 8--12 ms ParameterSmoother，避免拖动参数产生 click/pop。Process 热路径无 mutex、无日志格式化、无 JSON、无动态申请。

## 8. VoiceProcessor 接入

处理链现在为：

```text
Input
  -> Input Analysis Tap
  -> DC Blocker
  -> Pitch Detector
  -> Pitch Shift
  -> Formant Shift
  -> Parametric EQ
  -> Soft Haze
  -> Spatial Haze
  -> Limiter
  -> Output Analysis Tap
  -> Output
```

Haze 处理器有独立 Bypass API。两者采用并行 dry path，`VoiceProcessor::LatencyFrames()` 不因 Haze 的内部短延迟增加。

## 9. Spectrum Analyzer、FFT 与分析 Tap

上一阶段的通用 Spectrum Analyzer、Input/Output Analysis Tap、FFT Worker 线程模型保持不变。本阶段没有把 FFT 放入 Haze 或 Audio Callback，也没有让 UI 读取 Audio Buffer。现有 Analyzer 使用 Engine 内部的小型 radix-2 complex FFT（`SpectrumAnalyzer::Transform`），不是新增第三方依赖，因此本阶段没有额外 FFT 许可证；RNNoise 的既有 BSD-3-Clause 许可证仍由原有包内 `RNNoise-LICENSE.txt` 覆盖。

## 10. UIParametricEQEditor 与 Voice Mixer 页面

上一阶段 EQ 编辑器继续显示输入/输出频谱和所有 EQ Band。本阶段在 Voice Mixer 增加第五页：

```text
Noise | Pitch | Formant | EQ | FX
```

FX 页显示 Soft Haze 与 Spatial Haze 两个卡片，提供 Enable、Amount、Blur/Size、Diffusion、Damping、Width、Wet 控制。页面切换只改变可见性，不销毁编辑器；隐藏 FX 页时 Haze DSP 和 Analyzer 仍继续运行。没有添加 Haze 曲线。

## 11. Preset 格式与迁移

Preset schemaVersion 从 3 提升到 4，并在根对象增加：

```json
{
  "softHaze": {
    "enabled": false,
    "amount": 0.35,
    "blur": 0.35,
    "damping": 0.35,
    "wet": 0.20
  },
  "spatialHaze": {
    "enabled": false,
    "amount": 0.30,
    "size": 0.35,
    "diffusion": 0.50,
    "damping": 0.40,
    "width": 0.70,
    "wet": 0.12
  }
}
```

缺少字段的旧 Preset 自动使用关闭的 Neutral Haze；未知字段继续保留；保存仍采用原子替换。Voice Mixer 仍只负责组合和控制，DSP 逻辑留在 Engine。

## 12. 自动测试

新增 `xie_haze_effects_tests`，覆盖：

- DelayLine 环回、Reset、Mono/Stereo。
- All-Pass 稳定性和有限值检查。
- Soft Haze disabled dry path、enabled signal change、Metrics、Reset。
- Spatial Haze disabled dry path、enabled signal change、Width=0 单声道兼容、Metrics、Reset。
- Soft/Spatial Snapshot 连续发布各 1000 次。
- VoiceProcessor 整链 Haze 集成和有限值检查。
- 44.1 kHz Mono 与 48 kHz Stereo。

Voice Mixer 测试新增：FX 五页面切换、V4 Haze Preset roundtrip、未知字段保留、旧 Preset Neutral Haze 迁移。

本轮直接运行结果：

```text
Haze DSP: ... mono/stereo 44.1/48 kHz ... passed
VoiceMixer tests passed
```

## 13. 实时安全压力测试

`xie_audio_rt_safety_tests` 同时连续发布 Curve、EQ、Noise、Soft Haze、Spatial Haze 快照各 1000 次，并持续运行 AudioGraph/VoiceProcessor：

```text
0 allocations
deadline_misses=0
noise_rt_allocations=0
```

最近一次直接高优先级运行的最大 Process 时间约 `0.2812 ms`（48 kHz / 128 frames 的约 2.8 ms deadline 内）。普通 CTest 主机的 wall-clock deadline 统计可能受 Windows 调度抖动影响，最终判断以无分配的直接实时测试为准。

## 14. Benchmark

`XiEAudioBenchmark` 已增加：

- `voice_without_noise_eq_analyzer`
- `voice_eq1` / `voice_eq8` / `voice_eq16`
- `voice_soft_haze1`
- `voice_spatial_haze1`
- `voice_soft_spatial_haze`
- `voice_rnnoise_pitch_formant_eq16_analyzer_haze`

48 kHz / Stereo / 128 frames 的本机 Release 代表值（每个场景 1000 次，单位 ms）：

| 场景 | Avg | P95 | P99 | Max | RT Alloc | Deadline Miss |
|---|---:|---:|---:|---:|---:|---:|
| EQ16 | 0.0421 | 0.0627 | 0.0718 | 0.0922 | 0 | 0 |
| Soft Haze | 0.0286 | 0.0459 | 0.0493 | 0.0517 | 0 | 0 |
| Spatial Haze | 0.0286 | 0.0469 | 0.0541 | 0.0545 | 0 | 0 |
| Soft + Spatial | 0.0306 | 0.0502 | 0.0521 | 0.0549 | 0 | 0 |
| Full + Analyzer + Haze | 0.6544 | 0.8742 | 0.9938 | 1.0576 | 0 | 0 |

测试机的 128-frame deadline 约 2.667 ms；以上测试场景保留了余量。Analyzer Worker 时间与 Audio Callback Process 时间分开统计。

## 15. 实机 Voicemeeter 结果

上一阶段已完成 Voicemeeter 实机端点验证，Voice Mixer 的常见 VoiceMeeter/CABLE loopback 组合仍会自动关闭 Monitor，防止桌面音频和监听形成反馈回路。本阶段没有引入 Voicemeeter Remote API，也没有改动该保护逻辑。

Soft Haze / Spatial Haze 的新听感尚未在本轮用耳机完成，标记为 `Not Tested`；不能仅凭频谱或合成测试信号宣称人声听感通过。

## 16. 已知限制

- Haze 参数目前是 0--1 归一化控件，未提供更细的工程单位输入。
- Spatial Haze 使用固定四级短扩散网络，不是可变房间模型。
- Haze 内部延迟是 wet 分支的一部分，当前总延迟报告保持 dry-path 语义。
- Snapshot 保留策略与现有 EQ/Noise/Curve 处理器一致，集中在处理器生命周期内回收。
- 本轮未开发降噪新算法、动态 EQ、De-esser、压缩、AI EQ、Vocoder、BitCrusher、RingMod、长混响或任何 FabFilter 复刻视觉。

## 17. 下一阶段建议

在有人佩戴耳机完成人声听感后，再根据实际听感调整默认 Amount/Wet、扩散密度和诊断展示；之后可设计 Pre-EQ/Post-EQ 选择、Preset UI 分组和更精确的工程单位控件。不要在未完成人工听感前增加更长的空间尾或更多自动化效果。

## 18. Release 交付

最终 Release 已安装、压缩并运行 `XiEVoiceMixer.exe --self-test`（ExitCode=0）。

- 可执行文件：`vs_build/package/XiEVoiceMixer/XiEVoiceMixer.exe`
  - Size：1,938,944 bytes
  - SHA-256：`10E52A3CDF909B368690D3D1D9E0F23E27EBD048130FE00925E6ECA01E15A1EE`
- 压缩包：`vs_build/package/XiEVoiceMixer-Release.zip`
  - Size：2,025,328 bytes
  - SHA-256：`D7B3B574F0BB438418C69E8E6F565C09BF7A7A5455B232233B217189FDA2E3A2`
- FX 页面截图（本地验证产物）：`artifacts/XiE/Audio/HazeEffects/XiEHazeEffects-fx.png`
