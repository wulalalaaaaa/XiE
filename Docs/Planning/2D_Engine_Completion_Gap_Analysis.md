# XiE 2D Engine Completion Gap Analysis

Date: 2026-07-08
Scope: 2D engine/runtime/editor readiness for solo all-round game development.

## 1. Completion Definition

For XiE's 2D part, "complete enough" should mean:

1. A solo developer can create a small 2D game loop without editing engine internals.
2. Scene layout, movement tuning, collision bounds, visibility, and draw order can be changed as content.
3. Runtime changes are visible quickly through hot reload or a minimal editor loop.
4. Failures are recoverable and explain what went wrong.
5. The engine has clear boundaries: runtime data/systems in `Engine`, project policy in `XiE_Projects`, rendering in `Renderer`.

This does not require a full Unity-like editor, full ECS, complex physics, or production packaging yet.

## 2. Current State

### Runtime Loop

Current effective order:

```text
AssetHotReload -> SceneHotReload -> EnsureSpriteRefs -> Input -> Move -> Collision -> Camera -> RenderSync -> Render
```

Implemented:

- `Engine::Runtime2D::World2D`
- `Transform2D`
- `Velocity2D`
- `Collider2D`
- `SpriteRef2D`
- `Renderable2D { visible, layer }`
- `MovementSystem2D`
- `CollisionSystem2D`
- project-side `InputSystem2D`
- project-side `SceneLoader2D`
- project-side `SceneRuntime2D`
- project-side `AssetRuntime2D`
- project-side `CameraRuntime2D`
- project-side `RenderSync2D`

### Content Workflow

Implemented:

- `Config/Input.toml` for movement bindings and speed.
- `Assets/Scene/main.xscene2d` for initial entity placement.
- Runtime scene hot reload with validate-then-apply behavior.
- Runtime asset hot reload for current mesh/sprite/atlas/material/texture path.
- Player-follow camera runtime control through `IRuntimeRender2D::SetCamera2D`.

### Rendering

Implemented:

- Runtime mesh submission through `IRuntimeRender2D`.
- Entity visibility filtering.
- Stable layer-based visible entity ordering.
- Sprite/atlas asset parsing exists in the engine.
- Same-texture/same-atlas per-entity `SpriteRef2D` UV rendering MVP.
- Runtime/game camera setter exists and is wired to `Renderer2DFeature`.

Current important limitation:

- Camera behavior is not yet content-authored from scene/config, and there is no editor viewport camera.

This is now the most important authoring gap because future editor work needs stable entity identity, better validation, and scene-level metadata.

## 3. Remaining Gaps

### P0 - Scene Authoring And Diagnostics

Required before editor work can be useful:

- Entity names should be retained, not ignored after parse.
- Duplicate names should be reported.
- Invalid collider sizes should be reported.
- Scene reload diagnostics should expose attempt/success/failure counts and last error.
- Camera defaults should become scene/config content rather than hard-coded project defaults.

Recommended first rule:

```text
Keep the line-based scene format, but make identity, validation, and camera metadata explicit.
```

Why this is first:

- A minimal editor needs stable entity identity.
- Authoring errors should be caught before live world replacement.
- Camera defaults are level-local content and should not stay hidden in C++ defaults.

### P1 - Scene Authoring Maturity

Needed after basic authoring diagnostics:

- Scene format should remain simple until real editor needs force a richer format.
- Missing sprite paths should be reported in scene or asset diagnostics.
- Future format changes should preserve validate-then-apply reload behavior.

### P1 - Input Runtime Workflow

Current input is configurable at startup.

Remaining:

- Optional input config hot reload.
- Split raw key polling from action intent (`move_x`, `move_y`) before promoting anything to engine-level input.
- Avoid engine-level GLFW policy until the action shape is stable.

### P2 - Collision And Physics

Current collision is simple dynamic-vs-static AABB separation.

Needed for practical 2D games:

- Trigger/collision event reporting.
- Collision layers or masks.
- Swept collision or timestep policy for fast movement.
- Queries such as point test, AABB query, and raycast.

Do not build a full physics engine yet.

### P2 - Animation

Needed for sprite-based games:

- Frame animation data.
- Animation state update.
- Hot reload for animation descriptors.
- Minimal scene binding to animation clip/state.

This should come after per-entity sprite rendering is meaningful.

### P2 - Editor Minimum

The first useful editor should not be broad.

Minimum useful editor loop:

- Scene view.
- Entity list.
- Inspector for transform/collider/renderable/sprite.
- Save to `main.xscene2d`.
- Runtime observes the same file through hot reload.

Do not build editor UI before the runtime scene fields are coherent.

### P2 - Packaging/Templates

Needed for using XiE beyond the test project:

- 2D game template.
- Project generator or CMake helper.
- Dist output layout.
- Assets/Config copy rules generalized away from `test2Dgame`.

## 4. Recommended Milestone Order

### Milestone A - 2D Runtime Coherence

Status: In progress.

Done:

- Runtime world.
- Movement.
- Collision.
- Input config.
- Scene load/hot reload.
- Renderable visibility/layer.
- Same-texture/same-atlas per-entity sprite rendering.
- Runtime camera control.
- Player-follow camera.

Remaining:

- Scene authoring metadata and diagnostics.
- Content-authored camera defaults.

### Milestone B - Practical 2D Gameplay MVP

Includes:

- Camera follow/control.
- Collision events/triggers.
- Basic animation.
- Input action state.

### Milestone C - Minimal Editor MVP

Includes:

- Scene hierarchy.
- Inspector.
- Scene file save.
- Scene view controls.
- Runtime reload path reused from current scene runtime.

### Milestone D - Projectization

Includes:

- 2D template project.
- Project creation workflow.
- Packaging/copy rules generalized.

## 5. Next Plan

Next implementation task:

```text
Add a Scene Authoring And Diagnostics MVP.
```

Scope:

1. Preserve validate-then-apply scene hot reload.
2. Store parsed entity names in runtime data or a lightweight metadata layer.
3. Report duplicate names and invalid collider sizes.
4. Add scene-level camera defaults for follow/zoom/fallback center.
5. Expose scene reload diagnostics.
6. Do not build editor UI yet.

Suggested work:

- Extend `Entity2D` or project metadata so scene `name=` survives load.
- Update `SceneLoader2D` to validate duplicate names and positive collider half extents.
- Add an optional `camera` line to `main.xscene2d`, for example `camera follow_player=true zoom=1 fallback_x=640 fallback_y=360`.
- Add a small camera config handoff from scene load into `CameraRuntime2D`.
- Add reload counters to `SceneRuntime2D` and log concise diagnostics.

Acceptance:

- Build passes with `cmake --build --preset build-debug`.
- Existing scene still renders.
- Existing player movement, collision, camera, visible/layer, and sprite behavior remain intact.
- Duplicate names and invalid collider sizes fail scene load without replacing the live world.
- Valid camera defaults affect `CameraRuntime2D`.
- Reload diagnostics are visible enough for authoring iteration.
- No editor UI or full ECS is introduced.
