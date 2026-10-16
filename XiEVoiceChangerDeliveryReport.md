# XiE VoiceChanger Delivery Report

Date: 2026-07-29  
Validated platform: Windows 10 x64, MSVC 19.36, Windows SDK 10.0.22621.0

## 1. VoiceChanger project structure

```text
XiE_Projects/VoiceChanger/
├─ Source/
│  ├─ App/VoiceChangerAudioController.*
│  ├─ Preset/VoicePreset.*
│  ├─ UI/UICurveEditor.*
│  ├─ UI/VoiceChangerWindow.*
│  └─ Main.cpp
├─ Tests/VoiceChangerTests.cpp
├─ CMakeLists.txt
└─ README.md
```

The formal executable target is `XiEVoiceChanger`. A Release install produces
an application-local directory containing the executable and MSVC runtime
DLLs. No Runtime2D, collision, world, game, Voicemeeter SDK, GLFW, OpenGL, or
`xie_engine` DLL is present in that package.

## 2. Reused XiE Engine modules

- Curve model, sampler, LUT, and immutable snapshots;
- AudioCore Float32 buffers, parameters, and prepared SPSC transport;
- WASAPI Shared Mode backend;
- VoiceProcessor, Pitch/Formant processors, meters, and Diagnostics;
- XiE Win32 Window/Input contracts;
- XiE DrawList2D, text layout, Software2D renderer, and layered presenter;
- the existing EmoExec JSON parser and atomic-file replacement implementation,
  compiled as a small static settings-support target.

VoiceChanger does not copy or fork any backend, graph, detector, shifter, or
Curve interpolation implementation.

## 3. New project-layer code

The project layer contains only endpoint/application orchestration, view input
handling and drawing, the Curve editor adapter, preset schema mapping, and the
composition root. It does not add DSP features.

`AudioSpscRingBuffer` was promoted into AudioCore because capture/render
transport is reusable runtime infrastructure. Both Smoke and VoiceChanger use
that implementation.

## 4. CurveEditor architecture

`UICurveEditor` is generic and contains:

```text
CurveEditorModel
  └─ vector<CurveEditorTrack>
       └─ CurveModel
CurveEditorSelection
```

It already draws multiple visible tracks as overlays. Pitch and Formant use
separate editor instances and independent CurveModels, ranges, colors, and
snapshots. Left-click adds/selects, drag moves, and right-click deletes.
Rendering samples the existing XiE `CurveSampler`; the UI has no spline.

Edits mark a dirty generation. The view coalesces LUT rebuild/publication to at
most 30 Hz during dragging.

## 5. AudioController architecture

`VoiceChangerAudioController` is the only UI-facing audio API:

```text
RefreshDevices / SelectInput / SelectOutput
Start / Stop / Restart / Poll
ApplyParameters / ApplyCurveSnapshots
CaptureSnapshot
```

It owns the backend, capture/render streams, prepared Float32 buffer, SPSC
transport, VoiceProcessor, and Diagnostics. `IAudioBackend` injection makes
lifecycle tests independent of physical hardware.

The public state machine is:

```text
Stopped → Starting → Running → Stopping → Stopped
                         ├─ DeviceLost
                         └─ Error
```

Repeated Start is rejected. Device loss is observed by `Poll`; Refresh and
Restart remain available.

## 6. Device workflow

On refresh, the controller preserves an existing endpoint by stable ID,
otherwise selects the Windows default. Device selection is disabled while
running. Start opens one capture and one output endpoint at 48 kHz Float32 mono,
prepares VoiceProcessor and transport, then begins event callbacks. Stop joins
both streams before releasing their resources. Monitor gates the one output
endpoint; no second monitoring graph exists.

The minimal UI cycles endpoints when a device row is clicked. A searchable
drop-down is a future usability upgrade, not an audio-architecture dependency.

## 7. Snapshot update flow

```text
UI pointer edit
→ CurveModel edit
→ coalesced BuildCurveSnapshot (UI thread)
→ VoiceProcessor::PublishCurves
→ atomic raw snapshot read in Process
```

Parameters use AudioParameter atomics and existing smoothers. The audio thread
never reads UI objects, rebuilds LUTs, locks Curve state, or allocates.

## 8. Preset format

Preset JSON schema V1 stores:

```text
schemaVersion
name
parameters {
  wet, inputGain, outputGain,
  pitchSemitones, formantSemitones,
  bypass, monitor
}
pitchCurve[] { id, x, y, interpolation }
formantCurve[] { id, x, y, interpolation }
```

Files are atomically replaced and backed up using the existing XiE-project
writer. The parsed document is retained, so unknown fields survive load/save.
Automated tests verify parameter/curve roundtrip and unknown-field
preservation. The built-in Neutral preset is zero pitch/formant, Wet 1, and
unity gain.

## 9. RT performance

The realtime instrumentation ran Graph + Voice while another thread published
1000 Curve snapshots and parameter updates:

```text
Debug:   0 allocations, max Process 0.530 ms, 0 deadline misses
Release: 0 allocations, max Process 0.219 ms, 0 deadline misses
Format:  48 kHz stereo, 128 frames, deadline 2.667 ms
```

The existing 24-case Release benchmark remains 24/24 with zero Process
allocations and zero misses. The VoiceChanger UI publishes only from the
ordinary thread.

`RT allocations observed` in the UI represents runtime instrumentation reports;
the formal allocation guarantee comes from the instrumented test executable,
not from pretending that Release globally intercepts every allocator.

## 10. CTest result

Audio, VoiceChanger, and EmoExec Debug targets were rebuilt from current
source. Every registered test executable in the configured tree then ran:

```text
31/31 Passed
```

This includes the three Audio tests, two new VoiceChanger tests, all existing
XiE/UI tests, and the five EmoExec tests that previously showed Not Run.

The unchanged first 21 legacy XiE/UI test executables came from the existing
configured build tree. A clean build of the monolithic legacy `xie_engine`
target remains separately blocked by its pre-existing committed conflict
markers/missing legacy headers; VoiceChanger and the repaired EmoExec modular
targets do not link that DLL.

The EmoExec issue was repaired rather than hidden: its redundant dependency on
the conflict-damaged legacy `xie_engine` DLL was removed, the modular OpenGL
backend target was completed, and the executable/tests were rebuilt. All five
then passed.

## 11. Stress result

Automated:

- Start → Stop repeated 10 times with injected endpoint streams: Passed.
- Start → Restart repeated 10 times: Passed.
- enumeration and refresh: Passed.
- 500 rapid Curve/parameter publications: Passed.
- preset save/load/unknown-field preservation: Passed.
- hidden-window Software2D render smoke: Passed.
- 1000 concurrent Curve/parameter publications during RT Process:
  0 allocations, 0 deadline misses.

Actual endpoint exercise:

- two interactive Restart operations while routing a real microphone into
  VoiceMeeter: Passed at transport/state level;
- live Bypass, Wet, Pitch, Formant, Pitch Curve, and Formant Curve commands
  were accepted without restarting the streams;
- 3622 capture callbacks, 0 xruns, 0 dropped frames; max callback 0.572 ms.

The requested 30-minute human speaking/dragging test is **Not Tested**.

## 12. Physical Device Smoke

Endpoint enumeration found `Mic (MCHOSE V9 PRO)` and several physical render
devices. Direct speaker/headphone routing was deliberately **Not Tested**:
unattended microphone-to-speaker routing can cause acoustic feedback, and no
human listening confirmation was available.

Therefore physical listening, click/pop, acoustic dropout, and device-release
sound quality are **Not Tested**, despite automated lifecycle coverage.

## 13. Voicemeeter Smoke

Detected endpoints:

```text
Input-side:  VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)
Output-side: VoiceMeeter Input  (VB-Audio VoiceMeeter VAIO)
```

An actual 30-second route was executed:

```text
Mic (MCHOSE V9 PRO)
→ WASAPI capture
→ VoiceProcessor
→ VoiceMeeter Input
```

Result: process exit 0, 2998 callbacks, max callback 0.389 ms, 0 xruns,
0 dropped frames. No Voicemeeter SDK/API was used.

Status:

- WASAPI-to-Voicemeeter endpoint transport: **Passed**.
- Voicemeeter-to-headphone audible result: **Not Tested**.
- perceived continuity/quality: **Not Tested**.

## 14. Actual end-to-end latency

For the tested endpoint pair, WASAPI exposed/fell back to 1056-frame endpoint
buffers. The runtime estimate is:

```text
capture 22.0 ms + pitch algorithm 5.33 ms + output 22.0 ms
= 49.33 ms estimated
```

IAudioClient3 low-period initialization is attempted and safely falls back to
ordinary Shared Mode when the selected converted mono format cannot use a lower
engine period.

No loopback impulse or human latency measurement was performed. Actual
microphone-to-ear latency is therefore **Not Measured**, and 49.33 ms must not
be presented as a physical measurement.

## 15. Pitch listening issues

**Not Tested by listening.** The detector reported unvoiced/zero F0 during the
unattended run, consistent with silence or insufficient captured speech. The
existing known quality boundary remains: dual-delay pitch shift is a functional
low-latency first implementation, not a studio-quality phase vocoder.

Human acceptance must specifically check consonants, breath, low confidence,
continuous curve dragging, and fast bypass transitions.

## 16. Formant listening issues

**Not Tested by listening.** Formant remains explicitly **Experimental**.
The independent curve/control/data flow is complete, but the all-pass spectral
warp is not equivalent to LPC/vocoder-quality formant transformation.

## 17. Known limitations

- Physical and Voicemeeter audible listening: Not Tested.
- Thirty-minute manual stress/leak/dropout test: Not Tested.
- Measured end-to-end loopback latency: Not Implemented.
- Current selected endpoints expose approximately 22 ms buffers per side.
- The monolithic legacy `xie_engine` clean build still contains pre-existing
  source-conflict/missing-header blockers; the modular VoiceChanger package does
  not depend on it.
- UI endpoint selection is click-to-cycle rather than a searchable drop-down.
- Default-device notifications and automatic reconnect remain Not Implemented.
- Long-run independent endpoint clock drift compensation remains Not Implemented.
- Curve snapshots are retained for the VoiceProcessor lifetime; 30 Hz
  coalescing limits growth, but epoch reclamation remains future work.
- Pitch quality is first-version; Formant quality is Experimental.

## 18. Next optimization recommendations

1. Perform headphone and Voicemeeter human listening acceptance using the
   project README checklist.
2. Add an impulse/loopback latency harness and expose capture/ring/output
   components separately.
3. Negotiate the device mix format with IAudioClient3 before mono conversion so
   supported endpoints can reach their minimum shared engine period.
4. Add searchable endpoint popovers and persistent selected endpoint IDs.
5. Add default-device notification/recovery and asynchronous-clock drift
   compensation.
6. Replace the Formant implementation before quality-focused presets.
7. Promote the shared JSON/atomic settings utilities out of EmoExec's namespace
   into a public XiE Settings target without changing their behavior.
