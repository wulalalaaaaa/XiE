# UI2D Theme System

## Definition and resolution

`UIThemeDefinition` is the mutable, code-authored input. It contains a name, color/metric/animation tokens, and named style-class definitions. Phase 3H deliberately has no JSON loader, file watcher, editor, or platform integration.

`BasicUIThemeResolver` validates the complete definition before producing a shared `const ResolvedUITheme`. Resolution rejects empty or duplicate class names, non-finite values, invalid state bits, negative transition durations, opacity outside `[0, 1]`, and selectors that both require and forbid the same state.

Resolution performs hot-path preparation: class names become generational `UIStyleClassId` values, selectors receive class IDs and declaration order, rules are stably sorted by specificity, and the result receives a unique `UIThemeHandle` and revision.

`UIThemeRepository` optionally owns and shares resolved themes by handle. Windows can share one `shared_ptr<const ResolvedUITheme>` or use different themes independently.

## Tokens and built-in themes

`UIThemeTokens` groups backend-independent colors, metrics, and transition durations. `DefaultLight` and `DefaultDark` are code-defined validation themes with `Panel`, `Interactive`, `ImageItem`, and `FocusRing` classes. Legacy layout/render token fields remain exposed by `ResolvedUITheme` until those consumers migrate to `Tokens()`.

## Handles and theme switching

Theme and class handles contain index and generation. A class ID from one resolved theme cannot query another. During a runtime theme switch, `UIWindowRuntime` maps bound classes through their resolved names once; state updates never perform string lookup.

`UIThemeSwitchMode::Immediate` cancels only `InteractionStyle` animations and immediately resolves every node. `Animated` retains current style-runtime values and uses transitions from the new theme. Application animations are untouched in both modes. A theme switch may scan the scene; stable frames do not.

## Current boundary

Style rules control only position offset, scale multiplier, rotation offset, opacity multiplier, and color multiplier. They cannot change layout, visibility, focusability, hit shapes, textures, or content. JSON themes, widget-specific style models, shader materials, blur, glow, and business-domain styles remain outside phase 3H.
