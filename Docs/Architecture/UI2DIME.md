# UI2D Windows IME integration

XiE separates three layers:

- Platform input emits start/update/commit/end composition events.
- UI2D routes them to the focused node and keeps preedit outside committed
  text.
- `ITextInputMethod` controls activation, deactivation, cancellation and the
  screen-physical candidate position.

`WindowsTextInputMethod` owns all `HWND`, `HIMC`, `ImmSetCompositionWindow`
and `ImmSetCandidateWindow` usage. `UIWindowRuntime` only receives the generic
interface.

Both Win32 layered windows and Windows GLFW windows handle
`WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION` and
`WM_IME_ENDCOMPOSITION`. The GLFW adapter chains the original GLFW WndProc;
it does not replace or bypass GLFW input handling. A committed result records
its scalar count and suppresses the matching `WM_CHAR`/GLFW character
callbacks, preventing a composition commit from being inserted twice.

Candidate placement is:

```text
measured caret X
-> TextBox horizontal offset
-> scene/window logical coordinates
-> window DPI
-> screen physical window origin
-> ITextInputMethod::SetCaretRect
```

Automated tests inject pure composition events. Real installed IME candidate
behavior remains a manual Windows test because it cannot be made
deterministic in CTest.
