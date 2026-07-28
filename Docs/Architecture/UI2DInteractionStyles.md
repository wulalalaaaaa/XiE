# UI2D Interaction Styles

## State mask

The resolver derives `UIInteractionStateMask` from the read-only `UIInteractionSnapshot` and scene ancestry:

| State | Source |
|---|---|
| `Hovered` | `UIHoverTracker` |
| `Pressed` | `UIPressTracker` |
| `Focused` / `FocusWithin` | `BasicUIFocusManager` |
| `Disabled` | effective enabled ancestry |
| `Captured` | `UIPointerCaptureService` |
| `WindowInactive` | window focus state |

State is not copied into `UIVisual`. Input and focus route changes mark only affected nodes in `UIStyleDirtyQueue`; enabled changes mark the subtree, and theme/window-wide changes mark all nodes. With an empty queue, `BasicUIInteractionStyleResolver::Update` returns without traversing the scene.

## Selectors and specificity

A selector matches when its class ID is equal, every required bit is present, and no forbidden bit is present. Matching rules are applied from low to high specificity: required-state bit count, forbidden-state bit count, then declaration order. Later, more specific rules overwrite only properties they provide. Missing properties retain the prior merged value.

## Runtime data flow

Each styled node owns `UINodeStyleState`: class reference, previous interaction mask and target, current `UIStyleRuntimeProperties`, resolved theme revision, and node style revision. Equal properties do nothing; zero-duration changes write immediately; positive-duration changes start an animation using `Current + Replace + Forwards`.

The runtime order is input/focus/mutation/layout, interaction-style resolution, animator update, final transform update, and render-list construction. A newly observed state can start and advance its transition during the same frame. Hidden windows skip these steps; showing one marks styles dirty and resolves its current target.

## Independent animation channels

Animator ownership is keyed by `Node + Property + UIAnimationChannel`. Existing APIs default to `Application`; the style resolver alone uses `InteractionStyle`. Replace, Reject, Queue, and cancellation operate within the selected channel.

```text
position = applicationPosition + stylePositionOffset
scale = applicationScale * styleScaleMultiplier
rotation = applicationRotation + styleRotationOffset
opacity = clamp(applicationOpacity * styleOpacityMultiplier)
colorMultiplier = applicationColorMultiplier * styleColorMultiplier
```

`VisualSize` remains application-only. The unified result feeds parent/child transforms, clip geometry, hit testing, effective opacity, and every RenderBuilder visual path. Transform-affecting style values mark Transform, Visual, and HitTest; opacity and color mark only Visual; no style value marks Layout. Completed transitions retain their target, release their record, and let the window return to Static.
