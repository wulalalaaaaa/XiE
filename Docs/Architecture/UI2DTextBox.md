# UI2D single-line TextBox

`UITextBoxWidget` is a generic XiE Widget composed as:

```text
Root (clipped)
|- Selection
|- Committed text / placeholder
|- Composition preedit
`- Caret
```

It supports pointer click, shift-click and captured drag selection;
Left/Right/Home/End with Shift extension; Backspace/Delete; Enter submit;
Escape cancel; Ctrl+A/C/X/V/Z/Y; read-only mode; horizontal caret tracking;
and scalar-indexed public text, caret and selection APIs.

The Widget uses the routed input and focus systems. Wheel is not consumed, so
an ancestor ScrollView can handle it. Selection and caret are overlay nodes;
caret movement does not change committed text. The placeholder never enters
the editing model, selection or clipboard.

Focus activates `ITextInputMethod`, starts the Animator-owned caret blink and
updates the candidate rectangle. Focus loss, window hide and destruction
clear composition and pointer selection, deactivate IME, and cancel caret
animation. The current scheduler has no low-frequency wake primitive, so the
first implementation uses the existing Animator while focused. A later
generic scheduler optimization should redraw only at blink transitions.

Project validation uses `xie_ui2d_text_box_tests` and the backend-neutral
`xie_ui2d_text_box_test`.
