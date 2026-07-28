# UI2D Widget System

## Purpose and authority

The Widget layer is a lightweight composition API over `UIScene`. A Widget groups one or more `UINode` handles, installs reusable behavior, applies a style class, and exposes stable properties and signals. It does not introduce a second layout, hit-test, input, focus, animation, or render tree. `UIScene` remains the authoritative UI tree and every Widget ultimately operates through the existing per-window `UIWindowRuntime` services.

`UIWindowRuntime` owns one `UIWidgetRuntime`. The Widget runtime owns a registry and a deferred mutation queue, but it does not own a platform window or renderer backend. Consequently the same Widget definition is valid for OpenGL and Win32 Layered rendering.

## Handles, records, and ownership

`UIWidgetHandle` and `UIWidgetConnectionHandle` are generation-checked handles. Destroyed slots can be reused only with a new generation. Generations are allocated across registries so a handle created for one window cannot accidentally validate in another window whose local slot index happens to match.

Each `UIWidgetRecord` stores:

- its Widget parent and direct Widget children;
- the public `rootNode`, optional child mount node, and all internally owned nodes;
- lifecycle, visibility, enabled state, kind, and creation order;
- compact behavior state for Button, Toggle, or ProgressBar.

The registry also maps every owned node back to its Widget. Destroying a Widget recursively destroys its child Widgets, disconnects its listeners/signals, and destroys its Scene root. Scene subtree destruction removes the Widget's own internal nodes without touching unrelated Widgets or raw Scene nodes. A raw Scene node is still legal and does not need an owning Widget.

## Registry, connections, and mutations

`UIWidgetRegistry` provides generation validation, Widget parentage, node ownership, destruction, reparenting, and connection cleanup. It intentionally does not interpret input, run layout, or draw.

Connections wrap existing `UIEventListenerRegistry` handles. The callback captures a Widget handle and revalidates it at dispatch time; it never captures a Widget object address. Activated and checked-changed signals use the same generational connection table. Disconnect is idempotent from the caller's perspective: the first valid disconnect succeeds and a stale handle is rejected. Widget destruction disconnects every remaining connection.

Destroy, reparent, visible, and enabled requests enter `UIWidgetMutationQueue`. Event callbacks therefore cannot invalidate the registry while listener dispatch is traversing it. The queue has an empty fast path. A flush applies Widget ownership changes first, then the normal Scene mutation queue is flushed before layout and rendering. Node invalidation is reported back to the Widget runtime so external Scene destruction also cleans the owning Widget record.

## Common properties

Every built-in Widget accepts visible, enabled, focusable, tab index, layout, transform, and style class. These values update the Widget root node. Visibility and enabled inheritance continue to come from Scene effective state; Widget code does not duplicate state down its internal subtree.

An invalid style class does not prevent construction. The Widget remains structurally and behaviorally valid without themed interaction presentation.

## Built-in composition

- `PanelWidget`: one Panel or NineSlice root, optional rounded corners and child clipping; its root is the child mount point.
- `ImageWidget`: one `UIImageVisual`; it accepts a caller-owned texture handle, fit, and tint and never loads files.
- `TextWidget`: one `UITextVisual`; it accepts a caller-owned text layout and never creates fonts, text layouts, input, or IME state.
- `ButtonWidget`: a Panel/NineSlice root plus `UIButtonBehavior`; arbitrary child Widgets provide content.
- `ToggleButtonWidget`: Button behavior plus a semantic checked value.
- `ProgressBarWidget`: a non-focusable root with background and fill nodes. Fill uses anchor geometry for four directions and emits no custom renderer command.

The lightweight headers under `Widgets/Basic` and `Widgets/Behaviors` delegate to `UIWidgetRuntime`; they are not an inheritance hierarchy and own no backend resources.

## Button and Toggle behavior

Button uses routed `Click`, `KeyDown`, `KeyUp`, and `FocusLost` events from the existing input system. Pointer press/capture/click generation, hover, and focus remain owned by `BasicUIInputRouter` and `BasicUIFocusManager`. Click is observed during bubbling so a Text or Image child can be the physical hit target without masking its containing Button.

Enter activates on non-repeat key down. Space exposes Pressed on key down and activates on key up; focus loss clears the pending Space state. Disabled Widgets reject user and programmatic activation. `focusOnPointerDown=false` suppresses only the default pointer focus move while retaining normal press and click semantics. An activation guard rejects recursive `Activate` calls.

Toggle changes the `Checked` explicit interaction state on its root. The normal interaction style resolver reads that node state; it has no dependency on the Widget registry. Theme rules and transitions can therefore select Checked exactly like Hovered, Pressed, Focused, or Disabled. Checked never mutates the stored `UIVisual`.

## Progress behavior

Progress values are finite and clamped to `[minimum, maximum]`; ranges with `minimum >= maximum` are rejected. Left-to-right, right-to-left, bottom-to-top, and top-to-bottom update the fill node's anchors. Value sources, smoothing, and business progress are deliberately outside the Widget layer; callers may use the existing application animation channel when interpolation is needed.

## Runtime and multi-window boundary

Each window has an independent registry, listener set, input state, focus manager, animator, theme, and Widget handles. A hidden window discards non-retainable input and does not flush Widget mutations. A static window with no input, mutation, dirty Scene state, or animation does not request additional frames.

The relevant safe-point order is: input and focus requests, deferred listener work, Widget mutations, Scene mutations, layout, interaction styles, animator, a second Widget/Scene mutation pass, transforms, render building, and frame-activity calculation. Widget destruction therefore completes before the frame's render build.

## Deferred controls

This phase intentionally excludes TextBox, IME, ScrollView, ListView, virtualization, ComboBox, Menu, Tooltip, Popup, Dialog, drag and drop, MVVM, reflection properties, Theme JSON, file hot reload, and all application-specific Widgets.
