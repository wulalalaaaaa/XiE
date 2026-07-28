# UI2D ScrollView and Scrollbar Widgets

## Node and ownership structure

```text
ScrollView Root
├─ Viewport (clipChildren = true)
│  └─ ContentHost Panel Widget
├─ Vertical Scrollbar Track
│  └─ Thumb
└─ Horizontal Scrollbar Track
   └─ Thumb
```

Root uses the normal Widget layout contract. Viewport is a full anchored child and uses the existing scene-space rectangular clip. ContentHost keeps the caller-selected `Absolute`, `VerticalStack`, `HorizontalStack`, `Anchor`, or `Overlay` layout mode.

`ContentHost(scrollView)` returns the internal Panel handle. `AddContent` and `RemoveContent` use the normal deferred Widget reparent operation; content may be any Widget kind. Under the current registry contract a parent Widget owns its child Widget subtree, so destroying ScrollView also destroys content currently mounted in ContentHost. This is the existing ownership rule, not a second ScrollView-specific lifetime mechanism.

## Layout synchronization

After a successful layout pass, behavior reads:

- Viewport `arrangedRect` width and height;
- ContentHost desired/arranged size;
- generic `ComputeUIContentExtent(ContentHost)`.

The maximum of the valid layout results becomes content size. Extent computation runs only after layout revision changes. Offset is clamped when content shrinks or the viewport grows. Auto scrollbars are visible only when their axis has a non-zero maximum offset. The current implementation uses Overlay placement, so scrollbar visibility cannot reduce the viewport.

## Pointer drag and Button coexistence

PointerDown in Bubble records only a potential drag. It does not capture or consume the Button press. After scene-space movement crosses `dragThreshold`, the innermost pending ScrollView:

1. claims the pointer from ancestor pending views;
2. captures the pointer on ScrollView Root;
3. calls `UIEventContext::CancelClick(pointerId, button)` on the existing press tracker;
4. moves model offset opposite pointer motion;
5. exposes the explicit `Scrolling` Theme state.

A short press therefore produces the existing Button Click. A threshold-crossing gesture cancels only that press record; it does not fabricate PointerCancel or disturb unrelated pointers. Up/Cancel releases capture and either starts inertia or emits completion.

Window focus loss already routes PointerCancel to captured/pressed pointers through `BasicUIInputRouter`. Hiding the window clears drag state but pauses smooth/inertia state until shown.

## Scrollbar geometry and interaction

Scrollbar binds to a ScrollView handle and axis. Geometry comes directly from the bound model:

```text
viewportRatio = viewportLength / contentLength
thumbLength   = clamp(trackLength * viewportRatio,
                      minimumThumbLength,
                      trackLength)
thumbTravel   = trackLength - thumbLength
thumbPosition = normalizedOffset * thumbTravel
```

For non-scrollable content, Thumb fills Track. `Auto` hides Track; `Always` may show the full disabled geometry.

Thumb PointerDown captures on Thumb and records the initial normalized value. Move maps local Track distance to normalized model offset and updates immediately without inertia. Up/Cancel releases capture and emits completion. A Track Click before or after Thumb moves exactly one configured page (`viewportLength * pageStepRatio`); it does not center Thumb on the pointer.

Track and Thumb use ordinary Theme style classes. Hovered, Pressed, and Captured come from InputRouter. ScrollView Root adds the generic explicit `Scrolling` state during drag, smooth motion, and inertia.

## Programmatic API

`UIWidgetRuntime` exposes:

```text
CreateScrollView / CreateScrollbar
ContentHost / AddContent / RemoveContent
SetScrollOffset / ScrollBy
ScrollToStart / ScrollToEnd
ScrollNodeIntoView
GetScrollOffset / GetMaximumScrollOffset / GetScrollSnapshot
BindScrollbar
OnScrollChanged / OnScrollCompleted
```

`ScrollNodeIntoView` accepts only ContentHost descendants and uses layout-local rectangles. `Nearest` performs the smallest movement necessary; Start, Center, and End align explicitly. FocusGained bubbling calls Nearest when `bringFocusedNodeIntoView` is enabled, without changing focus itself.

