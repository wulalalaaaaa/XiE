# XiE Electronic Mist V1 Delivery Report

Date: 2026-08-09

## Scope

Electronic Mist V1 adds a subtle, voice-following electronic texture. It is a
parallel noise layer, not a replacement for the voice signal. Noise suppression,
STFT spectral mist V2, vocoder, bit reduction, ring modulation, and strong
distortion remain out of scope.

## 1. Algorithm design

The Engine uses eight logarithmically distributed band-pass follower bands
(120, 250, 500, 900, 1600, 3000, 6000, and 11000 Hz). A prepared noise
generator is shaped per band, multiplied by the smoothed voice envelope and
frequency curve, then mixed as a bounded parallel layer. Amount and Wet are
smoothed; Follow controls how strongly individual band envelopes contribute.

## 2. Why band-based instead of STFT

The first version avoids block FFT/IFFT and overlap-add. This keeps latency and
CPU predictable, avoids phase/OLA artifacts, and permits a zero-allocation audio
callback. The snapshot and processor boundary leaves room for a future STFT
implementation without changing the Voice Mixer contract.

## 3. Band count

Eight bands provide useful low/mid/high voice coverage with materially lower
callback cost than 32/64-band analysis. The count is a named Engine constant,
so a later benchmark can evaluate a 12-band variant independently.

## 4. Noise generator

`ElectronicMistNoiseGenerator` supports White, Soft/Pink-like, and Digital
character modes. It is initialized in `Prepare`, uses a deterministic xorshift
seed for tests, and never calls a system random API from the callback.

## 5. Voice-envelope following

Each band has attack/release smoothing. A broadband envelope and a faster voice
gate prevent audible noise while the input is silent, while fricatives without a
stable F0 can still activate the layer.

## 6. Noise Curve semantics

`ElectronicNoise` is a normal Curve model with X = logarithmic frequency from
20 Hz to 20 kHz and Y = 0..1 injection amount. The control thread converts it
to a 1024-point immutable LUT; the audio thread only samples that LUT.

## 7. Sharpness

Sharpness applies a bounded high-frequency tilt and a small digital-grain
character blend. It does not use clipping, bit reduction, or hard distortion.

## 8. Stereo processing

Band envelopes are shared between channels while noise streams use independent
channel state. Width controls a limited decorrelation mix and is clamped to the
safe range; mono remains a single deterministic path.

## 9. Silence gate

With Mist enabled and a silent input, the envelope release decays the layer to
near numerical silence rather than leaving a continuous hiss. The automated
silence test checks the settled tail after repeated 128-frame blocks.

## 10. VoiceProcessor position

The final Engine order is:

```text
Noise Suppression -> Pitch/Stabilization -> Formant -> Synthetic Voice
    -> Parametric EQ -> Electronic Mist -> Soft Haze -> Spatial Haze -> Limiter
```

Electronic Mist therefore follows the voice-shaping EQ and precedes the two
ambience layers. It remains independently bypassable.

## 11. Unified Editor track

The existing unified editor now exposes the `Electronic Noise` track. It keeps
the EQ logarithmic frequency axis but uses a 0..100% Y axis. Double-click,
drag, delete, and reset use the existing curve interaction and snapshot path.

## 12. FX page

The FX page contains four independent cards: Synthetic Voice, Electronic Mist,
Soft Haze, and Spatial Haze. Electronic Mist exposes Enable, Amount, Follow,
Sharpness, Width, Wet, Edit Noise Curve, and Reset Curve. No new top-level tab
was added.

## 13. Preset V6

The root preset schema is now V6 and writes an `electronicMist` object with
enabled, amount, follow, sharpness, width, wet, noiseType, and curve fields.
V1--V5 presets load with a disabled, zero-curve neutral mist. Unknown JSON
fields remain preserved and writes remain atomic.

## 14. Metrics

Voice diagnostics include Mist enabled/revision, Amount/Follow, Sharpness/Width,
band and active-band counts, broadband envelope/gate activity, Mist RMS, Voice
RMS, Mist-to-Voice ratio, and processing time. `VoiceProcessor::DumpVoiceState`
also exposes the same values for runtime diagnostics.

## 15. RT safety

All coefficient/filter/noise storage and the voice scratch buffer are prepared
before streaming. `Process` has no allocation, mutex, JSON, logging, or UI
access. Immutable snapshots are published on the control side and adopted at a
callback boundary. The Electronic Mist test publishes 1,000 snapshots and
verifies the final revision while processing continues.

## 16. Benchmark

`xie_audio_benchmark` now reports Electronic Mist processing time and adds
Mist-only, Mist+Synthetic+Haze, and full-graph Mist cases. Release CSV output is
generated with 48 kHz mono/stereo and 64/128/256/512-frame buffers. The final
measured values include:

- 48 kHz / stereo / 128 frames / Mist-only: 0.1193 ms average, 0.1229 ms P95,
  0.1439 ms P99, 0.1832 ms maximum (4.47% of the 2.667 ms deadline).
- 48 kHz / stereo / 128 frames / full graph with Mist, Analyzer and Haze:
  0.7145 ms average, 0.8552 ms P95, 0.8864 ms P99, 1.1301 ms maximum
  (26.79% of deadline).
- The measured Release benchmark reported zero allocations and zero deadline
  misses for every listed case. The full CSV is reproducible by running
  `vs_build\XiE_Projects\AudioRuntimeTests\Release\XiEAudioBenchmark.exe`.

## 17. Voicemeeter hardware

Not Tested in this environment. The existing WASAPI/Voicemeeter endpoint path
is unchanged; a physical MCHOSE/Voicemeeter route still requires manual
verification with headphones.

## 18. Electronic Mist listening

Not Tested. No claim is made about the final human listening balance without an
actual microphone/headphone session. The automated checks cover deterministic
noise, finite output, dry/disabled behavior, voice following, silence decay,
44.1/48 kHz, mono/stereo, snapshot switching, and reset.

## 19. Synthetic/Haze combination listening

Not Tested. Synthetic Voice, Soft Haze, and Spatial Haze remain independently
toggleable so the required A/B listening sequence can be performed later.

## 20. Known limitations

- V1 is a band-following texture and is not a true spectral-freeze/STFT effect.
- The eight-band bank is intentionally fixed for the first CPU/latency budget.
- Noise type is an Engine/preset field; the first UI keeps the main controls
  focused on Amount, Follow, Sharpness, Width, and Wet.
- No physical listening, Voicemeeter routing, or long-run hardware stress test
  was available here.

## 21. STFT Spectral Mist V2 recommendation

Only consider V2 after V1 is auditioned. If the band model cannot provide the
desired spectral grain, a worker-thread/windowed STFT could be added behind the
same snapshot and analysis interfaces. It should not be introduced merely to
increase visual complexity or before measuring the V1 CPU margin.

## Automated verification

The targeted Release CTest command and package self-test result are recorded in
the handoff message. The repository also retains the existing Noise, Synthetic,
Haze, VoiceMixer, and real-time safety tests; a full repository CTest run still
contains unrelated legacy targets that are not built in this configuration.

## Visual handoff

Release UI captures were produced from the packaged executable:

本地截图保存在被忽略的 `artifacts/` 下：

- FX page with Electronic Mist card: `artifacts/XiE/Audio/ElectronicMist/XiEElectronicMist-fx.png`
- Unified editor with Electronic Noise track active: `artifacts/XiE/Audio/ElectronicMist/XiEElectronicMist-unified-track.png`
- Diagnostics view: `artifacts/XiE/Audio/ElectronicMist/XiEElectronicMist-diagnostics.png`

## Release package

- `vs_build/package/XiEVoiceMixer/XiEVoiceMixer.exe`
  SHA-256: `7F22C1FB8F44CF2FCE89B275EB2AA52DF41EA51FCB9EC4030FA448BE0EA64B1E`
- `vs_build/package/XiEVoiceMixer-Release.zip`
  SHA-256: `E028D565C4CFA109DAA78896DE078A2D79365762BFAA20292487380072EBB19A`
- Package `--self-test`: exit code 0.
