# XiE Voice Mixer

`XiEVoiceMixer` is the thin desktop composition layer over XiE Curve and
Audio Runtime. It does not contain a second DSP, backend, or graph
implementation.

## Build and package

```text
cmake -S . -B vs_build
cmake --build vs_build --config Release --target XiEVoiceMixer
cmake --install vs_build --config Release --prefix vs_build/package/XiEVoiceMixer
```

The package contains `XiEVoiceMixer.exe` and the required MSVC runtime DLLs.
All other dependencies are Windows system DLLs.

## Controls

- Use the Input Device and Output Device dropdowns to select endpoints while stopped.
- Start opens both endpoints; Stop releases them; Restart performs Stop + Start.
- Monitor gates the single selected output endpoint. Do not select a desktop
  mix/loopback endpoint (for example `VoiceMeeter Output`) as Input while
  monitoring to its matching virtual input; that creates a feedback loop. XiE
  detects the common VoiceMeeter/CABLE loopback pair and disables Monitor for
  that run. Select the physical microphone as Input instead.
- Pitch, Formant, and EQ are mutually exclusive editor pages. Pitch/Formant
  retain their F0-to-shift Curve semantics. EQ uses independent filter bands,
  per-band/combined responses, and input/output spectrum overlays.
- FX is a separate page for Synthetic Voice, Electronic Mist, Soft Haze and Spatial Haze.
  Synthetic Voice is disabled by default and combines bounded F0 stabilization
  with a subtle optional micro-pitch layer. Haze effects remain independent of
  the Pitch/Formant/EQ curve editors; Electronic Mist uses the unified
  `Electronic Noise` frequency track, while there is intentionally no Haze
  curve.
- Electronic Mist is disabled by default. Its Amount, Follow, Sharpness, Width,
  Wet and noise type are published as an immutable Engine snapshot. The
  `Edit Noise Curve` button activates the logarithmic 20 Hz--20 kHz track; the
  curve is zero in a neutral preset and the effect is placed after EQ and
  before both Haze stages.
- In EQ, double-click empty space to add a Bell band, drag to edit frequency
  and gain, use the wheel for Q, right-click to change type, and Delete to remove.
- Wet, Pitch Shift, and Formant Shift use vertical faders with wheel and drag
  adjustment while streams keep running. Input/output gain stays at unity and
  is controlled by the external mixer (for example, Voicemeeter).
- Save/Load use `%LOCALAPPDATA%/XiE/VoiceMixer/Presets/Current.json`; the former
  VoiceChanger preset and UI settings are migrated once when needed.
- Reset Neutral restores zero pitch/formant/electronic-noise curves, Wet 1, and
  unity gain. `Reset Curve` clears only the Electronic Mist curve.
- The small `中文` / `EN` control switches UI language and remembers the choice.
- The window is resizable. Minimize sends it to the taskbar; `x` hides it while
  audio continues in the notification area. Right-click the tray icon and use
  Exit to stop audio and terminate the process.

## Manual listening acceptance

Use headphones first to prevent acoustic feedback.

1. Physical: select a microphone and headphones, Start, speak for at least
   five minutes, adjust every control, then Stop/Start/Restart.
2. Voice: drag both curve editors while speaking; verify stable F0, continuous
   transitions, consonants/breath, bypass, click/pop, and dropout behavior.
3. Voicemeeter: select `VoiceMeeter Input` as Output, route Voicemeeter to
   headphones, and repeat the same checks. No Voicemeeter SDK is involved.
4. Stress: run 30 minutes while dragging curves and toggling Bypass; watch
   callback maximum, deadline %, xruns, and underruns in Diagnostics.

Listening quality is not considered passed until a human performs these steps.
