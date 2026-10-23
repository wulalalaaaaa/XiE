# XiE Voice Mixer Noise Suppression - Headphone Listening Guide

Status: **Human Listening Quality: Not Tested**

This guide is the remaining human acceptance procedure. Use closed-back
headphones and avoid monitoring through speakers, which can add feedback and
make denoise artifacts difficult to separate from the room.

## Setup

1. Select the real microphone as Input and `VoiceMeeter Input` as Output.
2. In VoiceMeeter, keep input/output gains fixed for the entire comparison.
3. Start XiE Voice Mixer and confirm Noise state is Active, not Unsupported
   Format or Bypassed.
4. Keep Pitch, Formant, EQ, and global Wet fixed. First test with neutral
   Pitch/Formant and flat EQ; repeat later with the intended voice preset.
5. Record the microphone, the VoiceMeeter output, device names, sample rate,
   callback size, and XiE diagnostics. Do not normalize each take separately.

## Required environments

Test these separately and in useful combinations:

- computer fan;
- air conditioner or continuous room noise;
- keyboard typing;
- mouse clicks;
- short desk impacts;
- quiet room baseline;
- natural speech, breathy speech, sibilants, word endings, and pauses.

## A/B matrix

Read the same short passage at a stable distance and level for every row:

| Pass | Noise | Strength | Wet |
|---|---|---:|---:|
| A | Off | - | 0% |
| B | On | 25% | 100% |
| C | On | 50% | 100% |
| D | On | 75% | 100% |
| E | On | 100% | 100% |
| F | On | chosen value | sweep 0 -> 100% |
| G | Bypass toggle | chosen value | chosen value |

Repeat the chosen setting with the production Pitch/Formant/EQ preset. Confirm
that page switching does not change the sound and that Stop/Start/Restart does
not replay stale buffered audio.

## What to listen for

- How much steady background noise remains during pauses and speech.
- Whether consonant onsets or quiet word beginnings are removed.
- Whether word endings, breaths, and sibilants are truncated.
- Watery, metallic, chirping, pumping, or granular artifacts.
- Unnatural high-frequency loss or hollow voice tone.
- Keyboard/mouse/desk transient reduction and any transient smearing.
- Whether Pitch/Formant magnifies denoise artifacts.
- Clicks, pops, time jumps, or level jumps during Enable, Bypass, Strength, Wet,
  page switching, Stop/Start, and Restart.

## Pass record

Do not mark this section Passed until a person has monitored the output through
headphones.

```text
Tester:
Date:
Input device:
Output device:
Sample rate / callback frames:
Environment:
Chosen Strength / Wet:
Noise reduction judgment:
Speech preservation judgment:
Transient judgment:
Artifacts heard:
XRUN / dropped / deadline / RT allocation counters:
Human Listening Quality: Not Tested | Passed | Failed
Notes:
```

