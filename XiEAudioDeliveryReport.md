# XiE Audio Runtime Delivery Report

Date: 2026-07-29  
Platform validated: Windows 10 x64, MSVC 19.36, Windows SDK 10.0.22621.0

## 1. Added directories and files

The delivery adds 36 source/build/document files under:

```text
Engine/Curve/
Engine/Audio/Core/
Engine/Audio/Backend/WASAPI/
Engine/Audio/Graph/
Engine/Audio/DSP/
Engine/Audio/Diagnostics/
Engine/Audio/Voice/
XiE_Projects/AudioRuntimeTests/
Docs/Architecture/XiEAudioRuntimeArchitecture.md
```

The authoritative file list is represented by the CMake targets in those
directories. No Renderer, UI2D, or Runtime2D implementation was changed.

The root and Engine CMake files contained committed merge markers at the start.
Those two build descriptions were minimally resolved so the real modular target
layout and the Audio targets can be configured. The current source snapshot also
lacks `Engine/Runtime2D`; CMake therefore exposes a compatibility interface
target only when those source files are absent, without inventing Runtime2D code.

## 2. Module dependency relationship

```text
xie_curve
    ^
xie_audio_core
    ^
xie_audio_graph
    ^
xie_audio_dsp
    ^
xie_audio_voice

xie_audio_core <- xie_audio_backend_wasapi
xie_audio_core <- xie_audio_diagnostics
```

`XiEAudioSmoke` and `XiEAudioBenchmark` are composition roots. None of the audio
targets links Runtime2D, Renderer, UI2D, game-world, or EmoExec business targets.

## 3. Audio thread model

- Main thread: enumerate/select devices, open/close streams, build/prepare graph,
  build Curve LUTs, publish graph/curve/parameter state.
- WASAPI capture/render threads: MMCSS `Pro Audio`, event-driven endpoint I/O,
  prepared conversion buffers, callbacks, DSP, and lock-free SPSC transport.
- Worker thread: reserved extension point for future expensive Curve/model work.

All RT entry points are `noexcept`. Callback code does not log, access settings,
load resources, perform file I/O, lock a mutex, or allocate memory.

## 4. Buffer model

Internal audio is planar Float32. `AudioBuffer` owns contiguous
`channels * frames` storage, and `AudioBufferView`/`ConstAudioBufferView` are
non-owning strided views. `AudioBufferPool` prepares a fixed collection before
processing. Graph transition, DSP delay, detector, Voice dry, WASAPI conversion,
and Smoke ring buffers are all allocated in Prepare/open.

The backend boundary converts WASAPI interleaved Float32 to/from the planar
engine format. Dangerous zero-copy coupling to Windows buffers was deliberately
not introduced.

## 5. WASAPI implementation

Implemented:

- active capture/render endpoint enumeration and default endpoint identification;
- friendly name and mix-format reporting;
- default or explicit endpoint open;
- Shared Mode event-driven capture/render;
- Float32 mono/stereo requests at 44.1/48 kHz with WASAPI auto-conversion at the
  backend boundary;
- start, stop, reset, resource release, platform error codes, and stream latency;
- `Running`, `Stopped`, `DeviceLost`, and `Error` reporting;
- detection of endpoint invalidation/resource invalidation from WASAPI calls.

Known limitation: Exclusive Mode is represented by the public contract but
returns `UnsupportedMode`. Default-device change notifications and automatic
recovery are not implemented; the stream safely stops/reports and can be
reopened. Backend lifetime must enclose streams opened from it.

## 6. AudioGraph lifecycle

`AudioGraphBuilder` owns processors while building. `Build` calls every
processor's `Prepare` on the ordinary thread and yields an
`AudioGraphSnapshot`. `Publish` retains the immutable topology and publishes a
raw pointer with release/acquire ordering. The audio thread never ref-counts or
destroys a snapshot.

On replacement, the previous and new graphs are both processed into prepared
buffers for a configurable short crossfade. `Reset` resets every retained
processor. `Stop` must run after endpoint callbacks stop and then reclaims all
snapshots.

Known limitation: published graph snapshots are retained until `Stop`, trading
bounded session memory growth for an unambiguous no-reclamation RT path.

## 7. Curve/LUT mechanism

`CurveModel` supports sorted add/remove/move, stable IDs, linear and monotonic
Cubic Hermite interpolation, finite-data validation, duplicate-x rejection, and
clamped boundaries. Monotonic tangents prevent ordinary cubic overshoot.

`CurveLUT` has caller-selected size/domain (tests use 1024 and 2048), samples
with linear interpolation, and is owned by immutable `CurveSnapshot`. Pitch and
Formant curves atomically publish raw snapshot pointers and retain ownership on
the ordinary thread. No spline evaluation or allocation occurs in DSP.

Known limitation: curve snapshots are retained for the VoiceProcessor lifetime.
A future epoch-based reclamation API should be added for extremely long
continuous editing sessions.

## 8. Pitch Detector algorithm

The first detector is a low-latency YIN-style cumulative mean normalized
difference detector:

- prepared 1536-frame rolling mono analysis window;
- configurable 70–500 Hz range, threshold, RMS floor, and analysis hop;
- parabolic lag refinement;
- frequency, confidence, and voiced/unvoiced result;
- silence/low-amplitude rejection and low-confidence noise behavior.

The algorithm is behind `IPitchDetector` and can be replaced independently.

## 9. Pitch Shift algorithm

`IPitchShifter` separates the contract from `PitchShiftProcessor`. The delivered
algorithm is a real time-domain dual variable-delay pitch shifter with:

- two fractional read heads;
- sinusoidal overlap weights;
- circular prepared delay storage;
- smoothed ratio updates;
- maintained stream duration/sample count;
- declared algorithm latency of half the configured window.

Quality level: first low-latency implementation. It is not naive resampling and
does not change stream duration, but it is not a studio-quality phase vocoder.

## 10. Formant algorithm

`IFormantProcessor`, `FormantShiftProcessor`, and `FormantCurve` are independent
from pitch. The current processor uses two first-order all-pass sections and a
smoothed residual mix to warp broad spectral emphasis without changing duration.

Quality level: **Experimental**. This is a stable, low-cost spectral-warp first
implementation, not a high-quality LPC/vocoder formant transformer. The public
interface is ready for replacement.

## 11. VoiceProcessor chain

```text
Input gain
  -> DCBlocker
  -> PitchDetector
  -> PitchCurve/FormantCurve evaluation
  -> confidence Hold/Fallback
  -> PitchShiftProcessor
  -> FormantShiftProcessor (Experimental)
  -> Limiter
  -> Wet/Dry and output-gain smoothers
  -> Peak/RMS meter
```

`VoiceProcessor` is an `IAudioProcessor`, so it can be one serial graph node.
DC, detector, pitch, formant, and limiter have independent bypass controls.
Pitch and Formant curves, Wet, Gain, OutputGain, Pitch, Formant, and Bypass can
be updated without stopping the stream or rebuilding the application.

## 12. Snapshot thread-safety mechanism

- Graph: immutable prepared topology, atomic pointer publication, ordinary-thread
  retention.
- Curve: immutable LUT snapshot, atomic pointer publication, ordinary-thread
  retention.
- Parameters: lock-free atomic values captured once per callback. Capture is
  deliberately bounded and never spins waiting for a writer.
- Metrics: lock-free atomics; formatting/dumps occur only on ordinary threads.

The parameter capture may observe two independently updated parameters from
adjacent UI publications. A future atomic multi-parameter transaction can be
added if a UI operation requires coupled values.

## 13. RT safety guarantee

`xie_audio_rt_safety_tests` overrides global `new/new[]`, warms the prepared
Graph and Voice paths, then instruments 200 Process iterations. Result:

```text
RT Process paths: 0 allocations; noexcept; lock-free atomics
```

Compile-time assertions verify lock-free Float32, double, and graph-pointer
atomics on the validated x64 platform. This is development instrumentation, not
a claim that arbitrary future processor implementations are automatically safe.

## 14. Latency measurement

WASAPI reports capture/render stream latency through `IAudio*Stream::Latency`.
Voice reports algorithm latency frames; Smoke converts it to processing
milliseconds and combines:

```text
capture + algorithm processing + output = total estimated latency
```

Callback duration, maximum duration, xruns, dropped frames, and total estimated
latency are available through `AudioDiagnostics`/`DumpAudioState`.

The subsequent VoiceChanger validation added an endpoint-buffer fallback when
WASAPI reports zero stream latency. For the tested MCHOSE/VoiceMeeter pair this
produced a 49.33 ms estimate (22 + 5.33 + 22 ms). This is an estimate, not a
measured microphone-to-ear result.

## 15. Benchmark results

Command:

```text
vs_build/XiE_Projects/AudioRuntimeTests/Release/XiEAudioBenchmark.exe
```

Each case used 1000 measured callbacks after 50 warmups on the validation
machine. All 24 cases reported **0 Process allocations** and **0 deadline
misses**.

Voice graph summary:

| Channels | Frames | Deadline ms | Avg ms | P95 ms | P99 ms | Max ms |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 64 | 1.333 | 0.268 | 0.549 | 0.615 | 0.773 |
| 1 | 128 | 2.667 | 0.537 | 0.576 | 0.627 | 0.808 |
| 1 | 256 | 5.333 | 1.072 | 1.173 | 1.348 | 1.758 |
| 1 | 512 | 10.667 | 2.138 | 2.228 | 2.414 | 2.811 |
| 2 | 64 | 1.333 | 0.269 | 0.556 | 0.651 | 0.887 |
| 2 | 128 | 2.667 | 0.538 | 0.605 | 0.660 | 0.779 |
| 2 | 256 | 5.333 | 1.069 | 1.113 | 1.266 | 1.383 |
| 2 | 512 | 10.667 | 2.154 | 2.265 | 2.595 | 2.992 |

The basic DSP graph averaged 0.037–0.074% of its deadlines. Empty graph results
were at timer-resolution floor.

## 16. CTest results

New tests, rebuilt from current source:

```text
xie_audio_runtime_tests    Passed
xie_audio_rt_safety_tests  Passed
xie_audio_smoke_self_test  Passed
3/3 passed
```

Coverage includes Curve sorting/editing/interpolation/clamp/LUT/snapshot and
invalid data; mono/stereo buffers, views, clear/copy/pool; atomic parameter
updates and smoothing; empty/single/multi graph, bypass, publish/replace, reset
and prepare; Gain, ChannelMixer, DC blocker, Limiter and meter; 100/220/440 Hz,
silence, deterministic white noise and low amplitude; Pitch Curve/Pitch Shift;
Formant stability; Voice curves, live replacement, Wet/Bypass and metrics.

Whole-tree CTest observed 24 pass and 5 Not Run. The 21 non-audio passes used
pre-existing binaries. A current-source rebuild of `xie_engine` is blocked by
pre-existing committed merge markers in `Core/Application.*` and
`Renderer/Renderer.*`, plus missing `TextureAsset.h`, `Camera2D.h`,
`AssetManager.h`, and the absent Runtime2D tree. Therefore a clean-source claim
for the legacy suite would be false. These files were not modified because this
delivery explicitly forbids Renderer/Runtime2D business refactoring.

## 17. Audio Smoke result

`XiEAudioSmoke --self-test` passed, validating the prepared SPSC transport.
`XiEAudioSmoke --list` enumerated six active input endpoints and five active
output endpoints in the current session.

The executable supports:

```text
XiEAudioSmoke --list
XiEAudioSmoke --run --input N --output N --seconds 30
XiEAudioSmoke --run --input N --output N --seconds 30 --voice
XiEAudioSmoke --run --input N --output N --voice --interactive
```

Interactive mode supports Start, Stop, Restart, Bypass, Wet, input/output gain,
Pitch, Formant, Pitch Curve, Formant Curve, and metrics without rebuilding the
application or stream.

An actual 30-second MCHOSE microphone → VoiceProcessor → VoiceMeeter Input route
completed with exit 0, 2998 callbacks, maximum callback 0.389 ms, 0 xruns, and
0 dropped frames. Direct microphone-to-speaker listening remains **Not Tested**
because unattended speaker feedback is unsafe.

## 18. Voicemeeter result

No Voicemeeter-specific SDK or runtime dependency was added. The installed
`VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)` was detected as an ordinary
WASAPI render endpoint.

WASAPI-to-Voicemeeter endpoint transport: **Passed** for the 30-second run above.
Voicemeeter-to-headphone audible listening and perceived quality:
**Not Tested**.

## 19. Known limitations

- WASAPI Exclusive Mode: Not Implemented.
- Default-device notification and automatic reconnect: Not Implemented.
- Shared capture/render use separate endpoint clocks and an SPSC ring; long-run
  asynchronous-clock drift compensation/resampling is Not Implemented.
- Pitch Shift: functional low-latency first version, not studio-quality.
- Formant Shift: Experimental.
- Graph branches, sends, mixes, and multiple outputs: Not Implemented.
- Snapshot epoch reclamation during very long editing sessions: Not Implemented.
- Actual speaker/headphone listening, hot-unplug, and audible Voicemeeter
  validation: Not Tested.
- Legacy whole-tree rebuild is blocked by pre-existing repository conflicts and
  missing source files described in section 16.

## 20. Next VoiceChanger UI integration

Build a separate desktop composition target that links Platform/Core/UI2D,
`xie_curve`, Audio, WASAPI, DSP, Diagnostics, and Voice only. Keep it out of
Runtime2D.

Recommended first UI surface:

1. endpoint enumeration and explicit input/output selection;
2. Start/Stop plus visible stream state and error code;
3. Pitch/Formant Curve editors that rebuild LUT snapshots off the audio thread;
4. Wet, Bypass, Pitch, Formant, input gain, and output gain controls;
5. callback/max callback, xruns, F0/confidence, and total estimated latency;
6. an explicit feedback warning when physical input and speaker are selected.

Before calling that application production-ready, complete real endpoint
listening tests, hot-plug tests, clock-drift compensation, Voicemeeter routing,
and upgrade the Experimental Formant algorithm.
