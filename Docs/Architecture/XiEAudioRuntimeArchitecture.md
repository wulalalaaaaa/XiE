# XiE Audio Runtime Architecture

## Baseline audit

The repository already separates platform-neutral contracts, platform backends,
application runtime, renderer, UI2D, and Runtime2D into CMake targets. Audio
follows the same composition model and does not enter renderer or Runtime2D.

The baseline inspected before implementation includes:

- root and Engine CMake topology plus CTest registration;
- `ApplicationRuntime`, `ApplicationServices`, and window lifecycle;
- Foundation handles/math, Core logging, and project-owned settings, paths,
  resources, and services in EmoExec;
- existing Renderer/Renderer2D, UI2D, Runtime2D, and project test targets.

The checked-in root and Engine CMake files contained conflict markers while the
Git working tree was clean. The resolution keeps both the modular project/test
targets and the legacy `Runtime` executable, without changing their business
logic.

## Targets and dependency direction

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

xie_audio_core
    ^
xie_audio_backend_wasapi
```

`xie_audio_diagnostics` depends only on AudioCore. The smoke program is a
composition root and may link backend, graph, DSP, voice, and diagnostics.

## Threads and ownership

- Main/UI thread: endpoint selection, stream open/close, graph construction,
  curve/LUT construction, and immutable snapshot publication.
- Audio callback threads: endpoint I/O and prepared DSP only. No locks, logging,
  settings, resource access, file I/O, exceptions, or memory allocation.
- Worker threads: reserved for expensive future graph/LUT/model preparation.

Prepared graph snapshots are immutable. `AudioGraph` retains every published
snapshot until it is stopped or destroyed, so an audio callback can atomically
load a raw snapshot pointer without reference-count destruction on the real-time
thread. Publication does not mutate the active snapshot.

## Internal audio contract

All graph and DSP data is planar float32 with one sample rate and channel layout
per prepared graph. Backend conversion is restricted to the WASAPI boundary.
Buffers and processor state are allocated in `Prepare`; `Process` is `noexcept`
and operates only on prepared storage.

## First implementation quality levels

- Curve, buffers, parameters, graph, base DSP, diagnostics: production contract.
- WASAPI shared-mode capture/render: first production backend; exclusive-mode
  request is represented but explicitly rejected until implemented.
- Pitch detector: low-latency YIN-style cumulative mean normalized-difference
  implementation.
- Pitch shift: time-domain dual-delay, overlap-faded low-latency implementation;
  time scale is maintained, but quality is intentionally a first version.
- Formant shift: experimental all-pass spectral-warp implementation and remains
  independently replaceable from pitch shift.

## Haze DSP extension

The short ambience layer is composed from reusable Engine DSP primitives rather
than Voice Mixer-specific code:

```text
DelayLine -> AllPassDiffuser -> DiffusionNetwork -> DampingFilter
```

`SoftHazeProcessor` uses one short all-pass path for near-field softening;
`SpatialHazeProcessor` uses four short early-reflection stages with a stereo
mid/side width control. Both processors have a parallel dry path, so their
internal delays do not increase reported graph latency. `Prepare` owns all
delay/filter storage, `Publish` retains immutable configuration snapshots, and
`Process` performs only prepared, smoothed sample processing. The Voice
Processor order is Noise, Pitch, Formant, EQ, Soft Haze, Spatial Haze, Limiter.

## Synthetic Voice extension

Synthetic Voice is an Engine DSP facade, not a Voice Mixer effect
implementation. `PitchStabilizer` consumes the existing `PitchEstimate` and
adds a bounded, confidence-gated correction to the existing pitch-shift target.
`MicroPitchProcessor` then builds a dry-preserving parallel layer from the
existing `PitchShiftProcessor`; it does not create a second full pitch-shift
stage or an LFO chorus. The chain is therefore:

```text
Pitch Detector -> Pitch Curve + Stabilization -> one Pitch Shift
    -> Formant -> Synthetic/Micro Pitch -> EQ -> Haze -> Limiter
```

Synthetic configuration is published as an immutable `SyntheticVoiceSnapshot`
and adopted only at callback boundaries. Micro scratch buffers and shifter
state are allocated during `Prepare`; enable/amount/character/width/wet changes
use the existing `ParameterSmoother`. F0 control smoothing and the parallel
micro layer's algorithmic latency are reported separately in diagnostics.

## Electronic Mist V1

Electronic Mist is an Engine-only parallel texture layer. It deliberately uses
an eight-band band-pass follower bank rather than an STFT/overlap-add path in
V1. Each prepared band tracks the post-EQ voice envelope with attack/release
smoothing, while a fixed-seed noise generator produces soft, digital, or white
noise. The immutable snapshot carries the clamped Amount, Follow, Sharpness,
Width, Wet parameters and a 1024-point logarithmic 20 Hz--20 kHz noise curve.

The callback reads the adopted snapshot, reuses preallocated voice scratch and
filter state, and performs no allocation, locking, JSON, logging, or UI work.
Snapshot objects are retained on the control side using the same lifetime rule
as the other VoiceProcessor effects. Silence is gated by the broadband and
per-band envelopes so enabling the effect does not create a continuous noise
floor; stereo decorrelation is limited by Width and the mono path remains
deterministic.

The production chain is now:

```text
Noise Suppression -> Pitch/Stabilization -> Formant -> Synthetic Voice
    -> Parametric EQ -> Electronic Mist -> Soft Haze -> Spatial Haze -> Limiter
```

Electronic Mist is independent of the Noise Suppressor and is disabled with a
zero curve in a neutral preset. The Voice Mixer only binds the Engine
configuration, preset fields, FX card, and `Electronic Noise` track in the
unified editor; it does not contain DSP code.
