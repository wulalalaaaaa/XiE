# XiE Noise Suppression Delivery Report

Date: 2026-08-02  
Configuration: Windows 10 x64, MSVC 19.36, CMake Release  
Product: XiE Voice Mixer

## 1. Existing project audit

The implementation was based on the existing AudioGraph, immutable curve/EQ
snapshots, `AudioParameterStore`, UI2D input/rendering, WASAPI shared-mode
streams, spectrum analyzer, preset repository, and Voice Mixer controller.
No replacement AudioBuffer, graph, parameter store, curve publisher, widget
input system, or analyzer was introduced.

The actual reusable UI directory is `Engine/UI2D`, not `Engine/UI`. Voice Mixer
uses the existing JSON parser and atomic replacement writer through a small
static settings library; it does not link the EmoExec application runtime.

## 2. Verified uncertain items

- Voice DSP buffers are planar Float32. Voice Mixer currently requests 48 kHz,
  mono, Float32, 128 frames, but the WASAPI endpoint supplies its actual buffer
  size and capture packet sizes are not assumed to be fixed at 128.
- The tested MCHOSE input is 48 kHz mono. VoiceMeeter Input is 48 kHz stereo;
  the existing controller performs endpoint/channel adaptation around the mono
  VoiceProcessor path.
- Official RNNoise v0.2 builds as C source under the existing MSVC/CMake build.
- `rnnoise_get_frame_size()` returns 480 for the integrated version. The
  required input is mono, 48 kHz, PCM-scale float; XiE converts its normalized
  Float32 range at the wrapper boundary.
- The official release embeds its generated default model in
  `src/rnnoise_data.c`; no runtime model download or model file is required.
- Release callback sizes 64, 128, 256, 512, and a non-divisible 137-frame test
  were exercised. None is assumed to divide 480.

## 3. Selected denoising algorithm

The first implementation uses official Xiph.Org RNNoise. It is a lightweight
speech-oriented recurrent denoiser and is executed synchronously only when the
frame adapter has accumulated a complete algorithm frame. A worker thread was
not added because measured RNNoise frame cost remains well below the normal
48 kHz callback deadline; avoiding the worker also avoids a second queue,
additional buffering, and late-window policy.

## 4. Algorithm source, version, and license

- Upstream: https://github.com/xiph/rnnoise
- Release: v0.2
- Signed release commit recorded by upstream: `904a876`
- Official archive SHA-256:
  `90FCE4B00B9FF24C08DBFE31B82FFD43BAE383D85C5535676D28B0A2B11C0D37`
- License: BSD-3-Clause
- Vendored source modifications: none
- XiE-only addition: `ThirdLibraries/rnnoise/CMakeLists.txt`

The original `COPYING`, source record, and consolidated third-party notice are
retained in the repository and Release package.

## 5. Third-party integration

RNNoise and its embedded model are compiled into the static target
`xie_thirdparty_rnnoise`, which is linked through `xie_audio_dsp`. The package
does not contain or require an RNNoise DLL. On MSVC x64 the build selects the
official SSE2-compatible path using RNNoise's existing x86 architecture macro.
AVX and AVX2 are not forced, so unsupported AVX2 CPUs do not enter an AVX2-only
binary path.

## 6. Sample rate and frame format

The wrapper queries a 480-sample frame from the library and requires 48 kHz,
mono Float32 processing. XiE normalized samples are scaled by 32768 before the
RNNoise call and scaled back afterward; non-finite samples are sanitized and
the returned signal is bounded. A 44.1 kHz request enters an explicit
`UnsupportedFormat` pass-through state rather than silently disabling DSP.

For multichannel input, one coherent mono downmix is processed and restored to
all target channels. Independent RNNoise states are not run per channel, which
avoids left/right denoising divergence and image drift.

## 7. NoiseFrameAdapter design

`NoiseFrameAdapter` owns a preallocated input frame, algorithm output, fixed
processed FIFO, and fixed dry delay. It accepts arbitrary callback block sizes,
forms exact 480-sample RNNoise frames, and returns exactly the number of samples
received on every call. Prepare performs all container sizing; Process performs
no allocation, locking, file I/O, or container growth.

The long-run test processes 24,137 samples with 64, 128, 256, 512, and 137
sample blocks. It checks the exact delayed sequence, total frame conservation,
algorithm frame count, buffered remainder, reset, and restart. No sample loss,
duplication, or time-axis drift was observed.

## 8. Additional latency

The selected startup policy first emits the current dry signal, then performs a
10 ms crossfade to the fixed-delay processed signal when the first complete
480-sample output is ready. Long-run input and output lengths remain identical.

Frame accumulation/fixed adapter latency is 480 frames = 10.0 ms at 48 kHz.
The public RNNoise API exposes no separate lookahead value, so no unverified
lookahead number is added. Diagnostics show 0 ms while Noise is disabled and
10.0 ms while it is enabled and available. VoiceProcessor total algorithm
latency includes both this 480-frame delay and the existing pitch-shift delay.

## 9. Final VoiceProcessor chain

The implemented chain is:

`Input -> Input Spectrum Tap -> DC Blocker -> Noise Suppressor -> Input Gain -> Pitch Detector -> Pitch Shift -> Formant Shift -> Parametric EQ -> Limiter -> Output Spectrum Tap -> Output`

The raw input spectrum remains before denoising. Pitch detection and all voice
transform stages receive the post-noise signal. Noise has an independent
enable and bypass and is not tied to global Voice bypass or the visible page.

## 10. Snapshot and real-time safety

`NoiseSuppressorConfig` is validated on the non-audio thread and published as
an immutable retained snapshot. The audio callback acquires the newest raw
pointer atomically; published shared ownership remains retained so the audio
thread cannot observe a freed snapshot. Enable transitions reset only the
algorithm and frame adapter, not the AudioGraph.

Process is `noexcept`, uses preallocated buffers, contains no mutex, log
formatting, JSON, UI access, or file I/O, and was measured by the existing
global allocation instrumentation. Bypass/Wet transitions use the existing
10 ms parameter smoother.

## 11. Parameters and true Strength semantics

- `enabled`: creates an active denoise path when the format is supported.
- `wet`: current dry signal to denoised signal; 0 is original, 1 is fully wet.
- `strength`: RNNoise v0.2 has no model-strength control. XiE therefore blends
  the delayed dry signal with RNNoise output using
  `0.25 + 0.75 * strength`. Strength 0 is a conservative 25% denoise result;
  Strength 1 is the complete RNNoise result. It is not presented as an
  internal neural-model parameter.

Strength and Wet are finite-clamped to 0..1. The tests verify that boundary
Strength values produce different output rather than an inert UI control.

## 12. Noise page UI

Voice Mixer now has mutually exclusive `Noise`, `Pitch`, `Formant`, and `EQ`
pages. Page switching changes visibility without rebuilding processors, and a
hidden Noise page continues processing. The saved `LastEditorPage` accepts and
restores Noise.

The Noise page provides Enable, read-only RNNoise mode, independent Bypass,
Reset, Strength and Wet sliders with exact percentages, localized state,
VAD probability, average/peak attenuation, average/P99 process time, added
latency, and input/output levels. No fabricated frequency reduction curve is
drawn. Chinese and English UI variants are retained.

## 13. Preset version and migration

Voice Mixer preset schema is version 3. `noiseSuppression` contains `enabled`,
`mode`, `strength`, `wet`, and `bypassed`. Existing version 1/2 presets without
the object load with RNNoise selected, Noise disabled, Strength 1, and Wet 1.
EQ v2 migration is unchanged. Illegal numeric values are clamped; existing
unknown root and nested fields are preserved because save mutates the loaded
document. Atomic replacement and deterministic field writes remain in use.

Noise Reset restores disabled/RNNoise/Strength 1/Wet 1/not bypassed and resets
DSP state when it is enabled again. Device Restart calls VoiceProcessor Reset,
clearing the algorithm and frame FIFOs before the new stream.

## 14. Diagnostics fields

Diagnostics now publish and display Noise mode/state/enabled/bypass,
Strength/Wet, voice probability, objective full-band average/peak attenuation,
frame size, input/output buffered frames, added latency, algorithm average,
P95/P99/maximum time, algorithm/drop counts, snapshot revision, and RT
allocation count. Attenuation is a measured full-band input/output energy
ratio per algorithm frame; it is not a claimed per-frequency response.

Audio callback, RNNoise algorithm, and analyzer worker timing remain separate.

## 15. CTest results

Release regression on 2026-08-02:

| Test | Result |
|---|---:|
| xie_audio_runtime_tests | Passed |
| xie_parametric_eq_spectrum_tests | Passed |
| xie_audio_rt_safety_tests | Passed |
| xie_noise_suppression_tests | Passed |
| xie_audio_smoke_self_test | Passed |
| xie_voicemixer_tests | Passed |
| xie_voicemixer_ui_smoke | Passed |

Result: 7/7 passed in 1.04 seconds. Noise-specific tests cover all requested
adapter block sizes, non-divisible blocks, reset/restart, disabled/bypass,
Wet/Strength boundaries, silence, generated noise, generated voiced harmonics,
finite/peak checks, mono/stereo, 44.1/48 kHz, NaN/Inf, snapshot switching, and
1,000 Noise publications.

## 16. RT stress result

One concurrent Release test executed 1,000 Pitch/Formant curve publications,
1,000 EQ publications, 1,000 Noise snapshot publications, and 1,000 Noise
enable/bypass updates while processing the graph.

Result: 0 audio-thread allocations, 0 deadline misses, Noise RT allocations 0,
maximum measured combined Process time 0.1594 ms. The 1,000-switch UI test also
cycles Noise/Pitch/Formant/EQ without destroying DSP state. No frame drift,
use-after-free, out-of-bounds result, or test-detected data race occurred.

## 17. Benchmark result

Release benchmark covers mono/stereo, 48 kHz, and 64/128/256/512 frames for:
Voice without Noise, Noise disabled, RNNoise, RNNoise+Pitch,
RNNoise+Pitch+Formant, RNNoise+EQ 16 bands, and the full graph with analyzer.
All measured cases reported 0 RT allocations.

Representative 128-frame results:

| Graph | Channels | Avg ms | P95 ms | P99 ms | Max ms | Avg deadline | Miss |
|---|---:|---:|---:|---:|---:|---:|---:|
| RNNoise | 1 | 0.0455 | 0.1535 | 0.2319 | 0.3718 | 1.71% | 0 |
| RNNoise + EQ16 | 1 | 0.0558 | 0.1623 | 0.2392 | 0.3815 | 2.09% | 0 |
| Full graph | 1 | 0.6029 | 0.7779 | 0.9591 | 1.1783 | 22.61% | 0 |
| Full graph | 2 | 0.6170 | 0.7944 | 0.9235 | 1.0957 | 23.14% | 0 |

RNNoise algorithm averages were generally 0.14-0.20 ms per 480-sample frame;
the representative real-endpoint run was lower at about 0.064-0.065 ms.

Known benchmark observation: the ordinary-priority benchmark process recorded
two isolated wall-clock scheduling spikes across the full matrix: one miss in
64-frame stereo RNNoise (2.4261 ms maximum against 1.333 ms) and one miss in
256-frame stereo full graph (6.1702 ms maximum against 5.333 ms). The dedicated
RT stress test and actual WASAPI run both recorded zero misses/XRUNs. This is
reported rather than hidden; the benchmark executable does not assign itself
MMCSS audio priority.

## 18. Real Voicemeeter result

Actual endpoints were present and used:

- Input 4: `Mic (MCHOSE V9 PRO)`, 48 kHz, mono
- Output 0: `VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)`, 48 kHz, stereo

The live chain exercised Noise enable, Strength 0.25 -> 1.0, Wet 1.0,
independent bypass on/off, Stop, Start, and Restart. After Restart the state was
Active, frame size 480, added Noise latency 10 ms, total Voice algorithm latency
736 frames (Noise 480 + Pitch 256), Noise average 0.06499 ms, P95/P99 0.1 ms,
maximum 0.1062 ms, 0 Noise drops, 0 Noise RT allocations, 0 XRUNs, and 0 dropped
transport frames. Device enumeration/refresh succeeded.

The microphone produced no visible level during the automated run, so live
speech suppression and audible output quality were not inferred. Forced
DeviceLost recovery was not tested because disconnecting a user's active
device was outside safe automated scope.

## 19. Human headphone listening

**Human Listening Quality: Not Tested**

No claim of speech quality, transient handling, breath preservation, or absence
of metallic/watery artifacts is made from generated signals or a silent device
run. See `XiENoiseSuppressionListeningGuide.md` for the required A/B procedure.

## 20. Known artifacts and limitations

- RNNoise mode currently supports only 48 kHz. 44.1 kHz is explicit
  Unsupported Format pass-through; no engine-boundary resampler was added.
- Stereo is coherently downmixed and restored, which is appropriate for voice
  but intentionally does not preserve independent stereo ambience.
- Strength is conservative post-RNNoise blending, not neural model intensity.
- The 10 ms adapter latency is exact; the library exposes no separate lookahead
  query, so an unverified internal lookahead number is not fabricated.
- Synchronous RNNoise is retained based on current performance evidence. The
  two ordinary-priority benchmark scheduling spikes remain recorded above.
- No PESQ, STOI, DNSMOS, speech corpus, licensed human speech recording, or
  listening-panel test was performed. Objective tests use generated silence,
  noise, tones, and voiced harmonics.
- Full-band energy attenuation does not describe frequency-selective behavior.
- Forced DeviceLost recovery and real spoken-word Preset save/load were not
  manually tested in the live run; the corresponding non-device behavior is
  covered by automated tests.

## 21. Next-stage recommendations

1. Complete the headphone A/B matrix with real speech, fan/air-conditioner,
   keyboard, mouse, and desk impacts; preserve recordings and exact settings.
2. Repeat benchmark under the same MMCSS profile as the production WASAPI
   callback and collect longer P99.9/max traces before changing thread design.
3. Add an Engine-level 44.1 -> 48 kHz boundary resampler only if 44.1 kHz voice
   endpoints are a product requirement.
4. Consider a read-only post-noise spectrum only after listening confirms it
   adds diagnostic value; do not infer suppression quality from its appearance.
5. Consider perceptual metrics only with a clearly licensed speech/noise corpus.

## Final package

Path: `vs_build/package/XiEVoiceMixer/XiEVoiceMixer.exe`  
Size: 1,877,504 bytes  
SHA-256: `266627B6937A71A34BD494038AC939CE915A1689D2D06B3B70D700C539D98C54`

Archive: `vs_build/package/XiEVoiceMixer-Release.zip`  
Archive size: 1,999,543 bytes  
Archive SHA-256: `DFF42D37044A9A7F77C0C2439DBE7386CA6D325E137D65006CB42E9030FA2829`

The package self-test passed from a copied directory containing Chinese
characters and spaces. PE dependency inspection shows only Windows system DLLs
and the packaged MSVC runtime; there is no RNNoise/model DLL dependency.
