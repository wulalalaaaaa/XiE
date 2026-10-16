# UI2D Implementation Roadmap

## Completed foundation

- Phase 3A: UI2D contracts, scene/store lifecycle, dirty flags, mutation queue, runtime orchestration, service boundaries, and contract tests.
- Phase 3B: `BasicLayoutEngine` with deterministic measure/arrange and shared scene transforms.
- Phase 3C: `BasicUIHitTester`, hit shapes, clipping, painter traversal, and hit-test revisions.
- Phase 3D: `BasicUIRenderBuilder`, backend-neutral draw-list mapping, shared transform/clip semantics, and static draw-list caching.
- Phase 3E: `BasicUIInputRouter`, routed events, hover/press/capture/click state, and per-window input isolation.
- Phase 3F: `BasicUIFocusManager`, focus routes, focus-within, tab navigation, lifecycle fallback, and window-focus restoration.
- Phase 3G: `BasicUIAnimator`, generational animation handles, application property channels, fill/direction/loop/queue/cancel behavior, final-property resolution, and active/static scheduling.

## Phase 3H: Theme and interaction styles (complete)

Implemented:

- code-defined `UIThemeDefinition`, validated `BasicUIThemeResolver`, immutable `ResolvedUITheme`, theme repository, and `DefaultLight`/`DefaultDark`;
- generational `UIThemeHandle` and `UIStyleClassId`;
- interaction state masks, selectors, deterministic specificity, complete resolved styles, and per-property transitions;
- per-node style references/runtime state and an incremental `UIStyleDirtyQueue`;
- independent `Application` and `InteractionStyle` animation channels;
- one final composition path shared by transforms, hit testing, clipping, effective opacity, and rendering;
- Immediate and Animated theme switching, hidden/static behavior, dual-backend demonstration, and automatic regression coverage.

See [UI2DThemeSystem.md](UI2DThemeSystem.md) and [UI2DInteractionStyles.md](UI2DInteractionStyles.md).

## Phase 3I: Basic UI Widgets and behavior components (complete)

Implemented:

- per-window `UIWidgetRuntime`, generational Widget/connection handles, registry, ownership, lifecycle, and deferred mutation safety;
- common Widget properties and lightweight Panel, Image, Text, Button, ToggleButton, and ProgressBar composition;
- Button pointer/Enter/Space activation, connection-safe signals, recursive activation protection, and arbitrary child content;
- Toggle `Checked` as a generic explicit node interaction state consumed by the existing theme resolver and transition channel;
- ProgressBar range clamping and four-direction anchor geometry without renderer extensions;
- hidden/static scheduling behavior, automatic Widget tests, and one shared OpenGL/Win32 Layered validation scene.

See [UI2DWidgetSystem.md](UI2DWidgetSystem.md) and [UI2DWidgetLifecycle.md](UI2DWidgetLifecycle.md).

## Phase 3J: ScrollView, ScrollModel, and Scrollbar (complete)

Implemented:

- pure logical-pixel `UIScrollModel` and independent smooth/inertial physics;
- ScrollView Root/Viewport/ContentHost composition on the existing Widget and Scene lifecycle;
- generic layout content extent, clamped horizontal/vertical ranges, and one ContentHost runtime offset shared by rendering, clipping, and hit testing;
- Bubble Wheel handling with nested remaining-delta propagation, thresholded pointer drag, safe Button click cancellation, and pointer capture;
- Overlay Track/Thumb scrollbars, normalized thumb geometry, thumb drag, and page click;
- programmatic scrolling, bring-into-view, focus auto-scroll, `Scrolling` theme state, coalesced signals, and hidden/static/active scheduling;
- automatic runtime tests plus one shared OpenGL/Win32 Layered validation scene.

See [UI2DScrollSystem.md](UI2DScrollSystem.md) and [UI2DScrollViewWidget.md](UI2DScrollViewWidget.md).

## Phase 3K: Fixed-cell Virtualized Grid (complete)

Implemented:

- O(1) logical visible/overscan range calculation with explicit empty ranges;
- auto/min/max column calculation, fixed-cell positioning and virtual content extent;
- ScrollView snapshot/event integration without a second scroll runtime;
- active-slot reuse pool with generic create/bind/unbind callbacks;
- focus clearing before a focused slot is recycled;
- resize, 1.5-DPI logical layout, Hidden suppression and Static-clean behavior;
- 50k/100k automated/performance coverage with fewer than 100 realized slots.

See [UI2DVirtualizedGrid.md](UI2DVirtualizedGrid.md).

## Phase 3L: Text editing, TextBox, clipboard and IME (complete)

Implemented:

- validated UTF-8 scalar-indexed editing, selection and bounded undo/redo;
- measured text-layout caret stops and pointer hit maps;
- generic single-line TextBox with clipboard, focus, pointer selection,
  horizontal offset, placeholder, composition and lifecycle cleanup;
- generic text clipboard and input-method contracts plus Windows
  `CF_UNICODETEXT` and IMM adapters;
- chained GLFW/OpenGL and Win32 layered composition input;
- automated model, Widget, composition, lifecycle and performance coverage.

See [UI2DTextEditing.md](UI2DTextEditing.md),
[UI2DTextBox.md](UI2DTextBox.md) and [UI2DIME.md](UI2DIME.md).

Later work retains the architecture boundary: UI2D does not access business
data or project storage. Full grapheme segmentation, multiline/rich text,
autocomplete/popups and low-frequency timer wakes remain deferred.
