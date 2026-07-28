# UI2D Scroll System

Phase 3J adds backend-independent scrolling without introducing another layout, input, hit-test, or rendering tree. The system has three responsibilities:

- `UIScrollModel` owns viewport size, content size, offset, range, policy, normalized position, page step, and revision. It depends only on Foundation types and contains no Scene, Widget, platform, or renderer references.
- `UIScrollViewWidget` composes the existing Scene nodes, reads layout results, routes input, advances smooth/inertial motion, and writes the model offset to one Widget runtime transform.
- `UIScrollbarWidget` owns Track/Thumb nodes and binds to the same ScrollView model. It never stores an independent content offset or range.

All dimensions are logical pixels. DPI is applied by the existing render backends and never by `UIScrollModel`.

## Model and range

For each enabled axis:

```text
maximumOffset = max(contentSize - viewportSize, 0)
0 <= offset <= maximumOffset
```

Disabled axes always have a zero range. Negative and non-finite sizes normalize to zero; non-finite offsets normalize safely before clamping. Viewport/content changes immediately clamp the authoritative offset. A revision is incremented only when model state actually changes.

`UIContentExtent` is a generic layout helper. It reads direct child `arrangedRect` values and margins after layout; stack spacing is already represented by arrangement. It deliberately excludes animated position/scale, rotation-expanded scene AABBs, and render effects.

## Runtime offset composition

ScrollView does not mutate every content child. It sets one value on ContentHost:

```text
widgetRuntimeOffset = -scrollOffset

finalPosition =
    design/application position
  + animated application position
  + interaction-style position offset
  + widgetRuntimeOffset
```

`UpdateAnimatedTransforms` is the sole final transform path. RenderBuilder, scene-space Rect Clip, and HitTest therefore observe the same scrolled matrices. Application and Theme animation channels remain independent from scroll authority.

## Wheel and nested propagation

ScrollView listens during Bubble. Downward wheel input uses:

```text
scrollDelta = -wheelDelta * wheelStep
```

Vertical scrolling is preferred. Shift+Wheel, or a non-scrollable vertical axis with a scrollable horizontal axis, maps the Y wheel component to horizontal motion.

The routed `UIEvent::wheelDelta` is mutable remaining input. An inner view subtracts only the wheel fraction corresponding to its actual bounded movement. Any remainder continues to ancestor ScrollViews. A view already at its boundary consumes nothing.

## Smooth scrolling and inertia

`UIScrollModel` remains authoritative during motion. `UISmoothScrollState` interpolates from the current offset to a clamped target with the shared `EaseOutCubic` function. Consecutive wheel requests update the target and restart from the current model value, so there is no visual jump.

Drag velocity uses a fixed five-sample ring buffer and monotonic input timestamps. Inertia applies independent constant deceleration on X/Y:

```text
newMagnitude = max(oldMagnitude - deceleration * dt, 0)
```

Blocked axes stop immediately. There is no overscroll or rebound. Hidden windows do not call scroll update, so smooth and inertia states pause and resume using frame delta rather than wall time.

## Activity and events

Drag, smooth motion, inertia, and pending scroll signals contribute to `UIFrameActivity::hasActiveInteraction`. A settled ScrollView does not keep the window active.

`OnScrollChanged` is coalesced per update and only reports a real offset change. Immediate motion queues one completion; smooth/inertial motion queues completion when its state finishes. Callback-driven Widget destruction remains deferred through the existing mutation safe point.

## Current boundaries

- Overlay scrollbars are implemented. `ReserveSpace` is declared but intentionally deferred.
- Nested Wheel transfers remaining delta. Nested pointer drag is claimed by the innermost eligible view and is not handed to an ancestor mid-drag.
- There is no overscroll, elastic rebound, snap, multi-touch gesture, virtualization, ListView, or asynchronous data source.
- Content bounds are layout bounds, not rotated/animated visual bounds.

