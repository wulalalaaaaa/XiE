# XiE Synthetic Voice Delivery Report

Date: 2026-08-09  
Product: XiE Voice Mixer  
Stage: Synthetic Voice / 轻电子合成人声

## 1. Engine modules

Added `Engine/Audio/DSP/Synthetic/`:

- `PitchStabilizer`: bounded control-rate F0 stabilization with confidence gate, short hold and correction fade.
- `MicroPitchProcessor`: parallel, preallocated micro layer that reuses the existing `PitchShiftProcessor` implementation.
- `SyntheticVoiceProcessor`: facade that publishes immutable snapshots and combines stabilization with the micro layer.

The engine keeps UI, JSON, mutexes, logging and allocation out of the audio callback. Existing Pitch Detector, Pitch Shift, ParameterSmoother and snapshot retention are reused.

## 2. Pitch Stabilizer model

The stabilizer follows the detected F0 directly; it does not quantize to a scale and does not run a second full pitch shifter. Small deviations use a conservative low-pass, while larger movements use a faster trend coefficient. Correction is clamped to `0..24` cents internally and the public default is 12 cents.

The public control tuple is Stability, Response and Max Correction. Voice Mixer exposes the simplified Stability/Amount controls and keeps the response/maximum policy in the engine.

## 3. Confidence gate

Confidence below the existing voice threshold, unvoiced frames and silence hold the last control briefly, then fade correction to zero. This prevents voiced/unvoiced jumps and never injects a stale large correction after the hold expires.

## 4. Micro Pitch

Micro Pitch is a dry-preserving parallel layer. Two existing pitch-shift instances create small opposite detunes; the layer is mixed with smoothed Amount, Character, Width and Wet values. Character maps to approximately 2–12 cents. Width uses a mid/side layer mix and a mono input naturally collapses to a mono-compatible result.

Amount 0 or Wet 0 converge to the dry signal; bypass/enable changes are ramped over 12 ms. No chorus LFO, feedback loop or new delay implementation was added.

## 5. Synthetic Voice facade

`SyntheticVoiceProcessor` owns the stabilizer and Micro Pitch processor. `SyntheticVoiceConfig` contains Enabled, Amount, Stability, Electronic, Width and Wet. The first five controls are intentionally low intensity and default to disabled/neutral.

## 6. VoiceProcessor chain

The chain is now:

`Input → Input Analysis → DC Blocker → Noise Suppression → Pitch Detector → Pitch Curve + stabilization correction → one Pitch Shift → Formant → Synthetic/Micro Pitch → EQ → Soft Haze → Spatial Haze → Limiter → Output Analysis → Output`

Haze, EQ, Noise Suppression and analyzer behavior are unchanged. Hiding the FX page does not bypass Synthetic Voice.

## 7. Snapshot publication

`BuildSyntheticVoiceSnapshot` clamps all values before publishing. Main/worker code publishes `shared_ptr<const SyntheticVoiceSnapshot>`; the audio thread atomically adopts the raw pointer from the retained immutable set. Snapshot revisions are exposed in diagnostics and metrics. No snapshot object is destroyed on the audio callback.

## 8. Parameter smoothing and reset

Synthetic enable, Amount, Character, Width and Wet use the existing `ParameterSmoother`. Pitch correction is bounded and confidence-gated. Reset clears detector-control history, pitch-shifter delay state, micro scratch buffers and metrics.

## 9. Latency accounting

F0 analysis/control smoothing is reported separately from audio-path latency. The main path retains the existing Noise/Pitch latency. Micro Pitch reports its parallel layer latency (`windowFrames / 2`, 64 frames with the default 128-frame micro window); VoiceProcessor reports the longest audible path instead of double-counting parallel latency.

## 10. Metrics and diagnostics

Added Synthetic Enabled, Amount, Stability, Electronic, Width, Wet, Raw F0, Stabilized F0, Correction Cents, Confidence, Voiced/Held state, control smoothing, Micro detune, Micro delay, Micro latency, snapshot revision and process timing. The Voice Mixer Diagnostics strip includes Synthetic and Micro values.

## 11. UI page and controls

The existing FX page now has three independent cards: Synthetic Voice, Soft Haze and Spatial Haze. Synthetic exposes Enable, Amount, Stability, Electronic, Width and Wet. Pitch/Formant/EQ/FX/Noise page state remains mutually exclusive and page visibility does not destroy processors.

## 12. Preset format

Preset schema is now version 5 and adds a root object:

```json
"syntheticVoice": {
  "enabled": false,
  "amount": 0.0,
  "stability": 0.0,
  "electronic": 0.0,
  "width": 0.5,
  "wet": 1.0
}
```

Missing `syntheticVoice` loads as neutral disabled. Values are clamped, unknown root/object fields are preserved, and atomic save behavior is unchanged.

## 13. Automated DSP tests

`xie_synthetic_voice_tests` covers stable 220 Hz, jitter suppression, trend movement, confidence loss, correction limit, Stability 0, reset, Micro Pitch mono/stereo, 44.1/48 kHz, amount/wet dry behavior, finite output, metrics, reset and VoiceProcessor integration.

## 14. Model and preset tests

`xie_voicemixer_tests` covers schema V5 Synthetic Voice round-trip, legacy presets without the new object, unknown-field preservation, controller publication stress and existing curve/EQ/UI page behavior.

## 15. Real-time stress

`xie_audio_rt_safety_tests` publishes 1000 Curve, EQ, Noise, Soft Haze, Spatial Haze and Synthetic snapshots while continuously processing the graph. Result: 0 audio-process allocations, 0 deadline misses, lock-free atomics and no reported use-after-free/data-race symptoms in the stress run.

## 16. Benchmark

Release benchmark command: `vs_build/XiE_Projects/AudioRuntimeTests/Release/XiEAudioBenchmark.exe`.

Representative 48 kHz / stereo / 128-frame results on this machine:

| Case | Average callback | CPU of 2.667 ms deadline | RT allocations | Deadline misses |
|---|---:|---:|---:|---:|
| Voice base | 0.0284 ms | 1.06% | 0 | 0 |
| Synthetic stabilization + pitch detector | 0.5532 ms | 20.75% | 0 | 0 |
| Synthetic Micro Pitch | 0.0491 ms | 1.84% | 0 | 0 |
| Synthetic + EQ/Noise/Haze/Analyzer | 0.6406 ms | 24.02% | 0 | 0 |

The stabilization row includes the existing Pitch Detector analysis cost; the Micro Pitch row isolates the parallel layer. P95/P99 and 64/256/512-frame rows are emitted by the benchmark CSV output.

## 17. CTest result

Passed in Release:

- `xie_audio_rt_safety_tests`
- `xie_haze_effects_tests`
- `xie_synthetic_voice_tests`
- `xie_voicemixer_tests`

## 18. Manual device result

The application and package build successfully. Physical MCHOSE/Voicemeeter endpoint listening for this stage is **Not Tested** in this environment. Existing loopback feedback protection remains active and still forces Monitor off for the known VoiceMeeter/CABLE desktop-loopback route.

## 19. Headphone listening result

**Not Tested.** A human should A/B Synthetic Voice with Haze disabled first, then test Soft Haze and Spatial Haze combinations at conservative levels. Do not claim voice quality or click/pop acceptance from the automated signal tests alone.

## 20. Package

Release executable:

`vs_build/package/XiEVoiceMixer/XiEVoiceMixer.exe`

The package uses the XiE Voice Mixer name in the window, tray tooltip and installer directory. A ZIP is generated alongside the package for handoff.

SHA-256:

- `XiEVoiceMixer.exe`: `86B9552A1C299579BBC48ABBCA3ECD7E9777B6BB319E69952EF044358453406F`
- `XiEVoiceMixer.zip`: `76B6AA24159E11286FC63356B99AA35EAECA6FB4F3A2808A74E152085F8207F3`

FX page screenshot: `artifacts/XiE/Audio/SyntheticVoice/XiESyntheticVoice-fx.png`.

## 21. Known limitations

- Stabilization is a bounded F0 control aid, not scale correction or Auto-Tune.
- Micro Pitch uses the existing granular pitch shifter, so its wet layer has a measurable parallel latency.
- No Electronic Mist, spectral noise injection, vocoder, BitCrusher, RingMod, dynamic EQ, de-esser, compression or new reverb was added.
- Human microphone/headphone/Voicemeeter listening remains outstanding.

## 22. Next stage

Stop this stage here. The next separately designed stage may investigate Electronic Mist/Spectral Noise Injection after the Synthetic Voice package has received real-device and headphone feedback.
