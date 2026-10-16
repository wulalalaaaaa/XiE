# UI2D text editing

M8 adds a UI-independent single-line editing model in
`Engine/UI2D/TextEditing`.

## Index and encoding contract

Public text is UTF-8. `UITextIndex` values are Unicode scalar boundary
indices, not byte offsets. The decoder rejects malformed, truncated,
overlong, surrogate and out-of-range sequences. Navigation and deletion
therefore never split a UTF-8 sequence. Full UAX #29 grapheme segmentation
is intentionally deferred; combining sequences and ZWJ emoji can require
more than one navigation step.

`UITextEditingModel` owns committed text, anchor/active selection, scalar
navigation, replacement, deletion and a 64-snapshot undo/redo history. It
does not depend on Widgets, Scene, a window backend, clipboard or IME.
Composition preedit is deliberately stored outside this model.

## Text metrics

`TextLayoutBitmap::caretStops` retains one measured X coordinate for every
scalar boundary. On Windows the GDI layout implementation measures each
UTF-16 prefix with the selected font. `UITextHitMap` maps pointer X to the
nearest adjacent midpoint and maps scalar index back to measured X. No
average-character-width approximation is used.

Tests are in `xie_ui2d_text_editing_tests`; the performance executable is
`xie_ui2d_text_box_performance`.
