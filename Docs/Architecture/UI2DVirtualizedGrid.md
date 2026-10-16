# UI2D Virtualized Grid

`UIVirtualizedGridWidget` is a generic XiE UI2D widget adapter. It has no
knowledge of libraries, images, tags, thumbnails or EmoExec. The implementation
lives under `Engine/UI2D/Widgets/Virtualization`.

## Contract

`UIVirtualGridModel` is a pure O(1) layout model. It accepts logical viewport
size, vertical offset, item count and fixed-cell metrics. Its result contains
column/row counts, content extent and explicit count-bearing visible and
realized ranges. A count of zero is the unambiguous empty-range representation.

Column count is:

```text
floor((viewportWidth + horizontalSpacing) /
      (cellWidth + horizontalSpacing))
```

It is clamped by the configured minimum and optional maximum. Row count and
content height use ceiling division and do not create spacer nodes.

`UIVirtualizedGridBehavior` owns active slots and a reuse pool. Project code
supplies generic create/bind/unbind callbacks receiving only a widget handle
and `uint64_t` item index. A realized-range change binds entering indices and
recycles leaving indices. A width/metric change repositions existing slots.
When a focused slot leaves the realized range, focus is cleared before the
widget is hidden and pooled.

`UIVirtualizedGridWidget` binds the behavior to an existing XiE ScrollView.
It consumes the public `UIScrollSnapshot`, installs a scroll-changed listener,
and exposes `SetItemCount`, `SetMetrics`, `RefreshVisibleItems`,
`RefreshBindings` and a diagnostic snapshot.

## Scheduling and DPI

Unchanged row ranges do no create/bind/recycle work. Hidden window runtimes do
not refresh the behavior. The widget adds no animation or private polling, so a
clean ScrollView returns to XiE Static scheduling. Metrics are logical units;
the existing UI runtime applies DPI scale once during rendering.

## Verification

`xie_ui2d_virtual_grid_tests` contains 47 checks, including invalid metrics,
empty/partial grids, range clamping, overscan, resize and 50k/100k data. In the
standard 790×424 logical viewport the maximum realized count is 32. Release
model calculation is approximately 0.0001 ms per call. The companion
`xie_ui2d_virtual_grid_test` prints top/middle/bottom/resize diagnostics and
the 1.5-DPI logical-size invariant.
