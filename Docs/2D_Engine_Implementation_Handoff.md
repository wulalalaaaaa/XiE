# XiE 2D Engine Implementation Handoff

Date: 2026-07-08
Role: Shared handoff document between planning/architecture and implementation sessions.

## 1. North Star

XiE is aimed at solo or small-team all-round creators.

The 2D engine and editor should make it easier for one person to move between programming, level design, art assets, animation, debugging, and iteration without constantly switching mental contexts.

When design tradeoffs appear, prefer:

1. Lower context switching for a single full-stack creator.
2. Faster feedback and iteration.
3. Runtime stability and recoverability.
4. Clear module boundaries.
5. Feature completeness.

Avoid large abstractions unless they clearly reduce repeated work or make iteration faster.

## 2. Current Project Memory

Read these files first when resuming work:

- `Docs/XiE_2D_Project_Onboarding_Snapshot.md`
- `Docs/xie-single-author-principles.md`
- `2D_MinimalLoop_Architecture.md`
- `2D_MinimalLoop_Interfaces.md`
- `Docs/XiE_Framework_Annotated_QuickRef.md`
- `Docs/2D_MinimalLoop_Verification.md`

Current structure:

- `Engine` owns reusable engine infrastructure.
- `Engine/Launch` owns the shared executable entry and game bootstrap.
- `XiE_Projects/test2Dgame` owns the current sample 2D game project.
- `GameStartupDesc` and `CreateGameApplication()` are already the project-to-launch bridge.
- The current 2D runtime loop exists in `XiE_Projects/test2Dgame/Source/App/Runtime2D`.

Current minimal loop:

```text
AssetHotReload -> SceneHotReload -> EnsureSpriteRefs -> Input -> Move -> Collision -> Camera -> RenderSync -> Render
```

Verification status:

- `Docs/2D_MinimalLoop_Verification.md` records successful configure/build.
- Short runtime smoke test passed.
- Hot-reload touch path passed.
- Interactive movement/collision still needs a manual desktop confirmation when convenient.

## 3. Next Implementation Plan

### Goal

Make scene authoring safer and more editor-ready without building the editor UI yet.

This is the next step because runtime camera control is now implemented. The current `.xscene2d` file is useful, but it still drops entity names after parse, lacks duplicate-name diagnostics, lacks collider sanity checks, and does not carry camera defaults as content.

Target edit loop:

```text
edit scene identity/validation/camera defaults -> save -> validate -> either apply safely or preserve live world with clear diagnostics
```

Limit this to authoring metadata and diagnostics. Do not build editor UI, prefab systems, or scripting yet.

### Non-Goals

- Do not introduce a full ECS yet.
- Do not build the editor UI yet.
- Do not build a full engine-level Scene system yet.
- Do not implement multi-texture batching.
- Do not implement per-entity material switching.
- Do not redesign the renderer backend.
- Do not add a full editor viewport/camera tool yet.
- Do not introduce camera stacks, render targets, split screen, or cinematic timelines.
- Do not support nested prefabs, hierarchy, scripting, or components beyond the current minimal 2D data.
- Do not move all gameplay setup code into the engine.
- Do not put scene, physics, or gameplay logic under `Renderer`.
- Do not perform a large folder-wide rewrite.

## 4. Concrete Task For "2D Engine Implementation"

Implement a Scene Authoring And Diagnostics MVP.

### Suggested Scope

Runtime data and scene parsing:

```text
Engine/Runtime2D/Types.h
Engine/Runtime2D/World2D.h
XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneLoader2D.*
XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneRuntime2D.*
```

- Preserve parsed entity names in runtime data or lightweight project metadata.
- Reject duplicate entity names during scene load.
- Reject non-positive collider half extents.
- Add scene reload diagnostics counters.
- Preserve validate-then-apply behavior.

Project runtime work:

```text
XiE_Projects/test2Dgame/Source/App/Test2DGameApp.*
XiE_Projects/test2Dgame/Source/App/Runtime2D/CameraRuntime2D.*
XiE_Projects/test2Dgame/Assets/Scene/main.xscene2d
```

Suggested behavior:

- Add an optional scene-level `camera` line.
- Suggested first shape:

```text
camera follow_player=true zoom=1 fallback_x=640 fallback_y=360
```

- Hand valid camera defaults to `CameraRuntime2D`.
- If camera defaults are invalid, reject the scene load and preserve the live world during hot reload.
- Keep the camera update order unchanged.

Keep ownership simple:

- `Engine/Runtime2D` continues to own generic `World2D`, `MovementSystem2D`, and `CollisionSystem2D`.
- `test2Dgame` continues to own the first scene format and scene diagnostics policy.
- Promote only stable scene data/policy into engine-level scene services later.

### Acceptance Criteria

1. Project configures and builds with the existing presets:

```powershell
cmake --preset vs2022-x64-debug
cmake --build --preset build-debug
```

2. `test2Dgame` still runs the same minimal loop.
3. Existing scene still renders.
4. Scene `name=` survives parse and can be queried or logged.
5. Duplicate names fail scene load with a clear message.
6. Non-positive collider half extents fail scene load with a clear message.
7. Optional scene camera defaults affect `CameraRuntime2D`.
8. Failed hot reload preserves the previous live world and previous camera config.
9. `visible`, `layer`, per-entity sprite UV, and player-follow camera behavior remain intact.
10. Runtime mesh submission remains single-mesh.
11. `Test2DGameApp::OnUpdate` ordering remains:

```text
AssetHotReload -> SceneHotReload -> EnsureSpriteRefs -> Input -> Move -> Collision -> Camera -> RenderSync
```

12. No renderer backend multi-texture API is introduced.
13. No full ECS or editor UI is introduced.
14. Update this handoff document with:
   - files changed
   - build result
   - whether short runtime smoke test passed
   - whether invalid scene validation smoke test passed
   - next recommended plan

### Why Scene Authoring Next?

The 2D runtime loop can now load a scene, move, collide, render sprites, layer visibility, and follow a camera. The next bottleneck is not raw runtime capability; it is authoring correctness.

A minimal editor needs stable identity and validation before UI controls are useful:

- entity names must survive load
- invalid content must fail safely
- camera defaults should be content
- reload diagnostics must be visible

## 5. Design Notes

Keep the first extraction intentionally boring. Data-only shared types are a low-risk move, and they create a stable vocabulary for future systems:

- scene serialization
- editor inspector
- collision/runtime debugging
- prefab or template entities
- runtime hot reload

The preferred direction is:

```text
Project Runtime2D prototype
  -> Engine Runtime2D data vocabulary
  -> Engine Runtime2D services
  -> Scene file format
  -> Minimal editor loop
```

This keeps the engine grounded in working game behavior instead of designing abstractions in the air.

## 6. Handoff Protocol

Planning session responsibilities:

- Keep this file updated with the next implementation target.
- Preserve the north star and complexity constraints.
- Choose one small, verifiable next step at a time.

Implementation session responsibilities:

- Read this file before starting.
- Execute the current concrete task.
- Update the "Implementation Notes" section below after work is done.
- Do not silently broaden scope.

## 7. Implementation Notes

### 2026-07-08 - Planning analysis after Camera/View Runtime Control MVP

Status: Planning update completed.

Observed code state:

- `IRuntimeRender2D` exposes `SetCamera2D(float centerX, float centerY, float zoom)`.
- `BasicRenderer` forwards `SetCamera2D` to `Renderer2DFeature`.
- `Renderer2DFeature::SetCamera` applies `Camera2D::SetPosition` and `Camera2D::SetZoom`.
- `Renderer2DFeature::SetCamera` converts runtime center semantics into current top-left/y-down canvas semantics.
- `CameraRuntime2D` exists in the sample project.
- `CameraRuntime2D` defaults to `follow_player=true`, `zoom=1.0`, and fallback center `(640, 360)`.
- `CameraRuntime2D` warns and falls back for missing player or invalid zoom.
- `Test2DGameApp::OnUpdate` calls camera sync after collision and before render sync.
- `Test2DGameApp` also currently integrates the experimental Story runtime and F5/F6/F7 demo controls.

Implementation verification reported by implementation session:

- Camera/View Runtime Control MVP implemented.

Decision:

- Next step should be Scene Authoring And Diagnostics MVP.
- Reason: runtime coherence is now strong enough for a minimal loop, but editor-readiness is blocked by weak scene identity, validation, diagnostics, and content-owned camera defaults.

Current next task:

- Preserve entity names from `main.xscene2d`.
- Add duplicate-name and collider-size validation.
- Add optional scene-level camera defaults and feed them into `CameraRuntime2D`.
- Add scene reload diagnostics counters.
- Keep editor UI, ECS, prefab, scripting, and format migration out of scope.

### 2026-07-08 - Camera/View Runtime Control MVP (plan completion)

Status: Completed.

Files changed:

- Runtime render API:
  - `Engine/Core/GameApp.h`
- Renderer forwarding:
  - `Engine/Renderer/Renderer.h`
  - `Engine/Renderer/Renderer.cpp`
- Renderer camera application:
  - `Engine/Renderer/Feature/Renderer2DFeature.h`
  - `Engine/Renderer/Feature/Renderer2DFeature.cpp`
- Project camera runtime:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/CameraRuntime2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/CameraRuntime2D.cpp`
- Project app loop:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.h`
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`
- Project CMake:
  - `XiE_Projects/test2Dgame/CMakeLists.txt`

Behavior change summary:

- Game/runtime code can set active 2D camera center and zoom through `IRuntimeRender2D`.
- `BasicRenderer` forwards camera control into the active 2D feature.
- `Renderer2DFeature` reuses existing `Camera2D`.
- Sample project camera follows the player by default.
- Invalid zoom and missing player produce fallback/warning behavior.
- Camera sync runs after collision and before render sync.

Acceptance check:

- No editor viewport introduced.
- No camera stack introduced.
- No renderer backend redesign introduced.
- Existing mesh submit/clear path remains intact.

Build/test verification:

- `cmake --build --preset build-debug` passed on 2026-07-08.
- `xie_story_tests.exe Assets/Story` passed on 2026-07-08 with 33 checks.

### 2026-06-19 - Planning analysis after SpriteRef2D rendering MVP

Status: Planning update completed.

Observed code state:

- Per-entity `SpriteRef2D::spritePath` rendering MVP is complete for the current same-texture/same-atlas path.
- `Renderer2DFeature` treats submitted runtime UVs as final and does not apply the global sprite UV rect to runtime meshes.
- `AssetRuntime2D` tracks scene-referenced sprites/atlases and exposes them to `RenderSync2D`.
- `Test2DGameApp` calls `EnsureSpriteRefs` after scene hot reload.
- `RenderSync2D` resolves per-entity sprite UVs, preserves `visible` filtering, and stable-sorts by `layer`.
- The renderer has `Camera2D`, but `IRuntimeRender2D` only exposes runtime mesh submit/clear.
- `Camera2DController` exists, but it is not currently wired into `Renderer2DFeature`. If wired later, avoid stealing WASD from gameplay by default.

Build verification reported by implementation session:

- `cmake --build --preset build-debug` passed.

Decision:

- Next step should be a single-camera runtime control MVP.
- Reason: scene/render sprite semantics are now honest enough; the next workflow blocker is that game/runtime code cannot explicitly own the 2D view.

Current next task:

- Add minimal camera control to `IRuntimeRender2D`.
- Forward camera center/zoom through `BasicRenderer` and `Renderer2DFeature` to existing `Camera2D`.
- Add project-side camera runtime behavior that follows the player with a fixed zoom.
- Keep editor viewport, camera stacks, and cinematic features out of scope.

### 2026-06-19 - Per-entity SpriteRef2D rendering MVP (plan completion)

Status: Completed.

Files changed:

- Renderer runtime UV behavior:
  - `Engine/Renderer/Feature/Renderer2DFeature.cpp`
- Project asset runtime:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/AssetRuntime2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/AssetRuntime2D.cpp`
- Project app hot-reload wiring:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`
- Project render sync:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/RenderSync2D.cpp`
- Sample content:
  - `XiE_Projects/test2Dgame/Assets/Sprite/checker_full.xsprite`
  - `XiE_Projects/test2Dgame/Assets/Scene/main.xscene2d`

Behavior change summary:

- Runtime mesh UVs submitted by `RenderSync2D` are treated as final.
- Scene-referenced sprite descriptors are tracked and made available after scene hot reload.
- Entities can resolve their own `sprite.spritePath` to UV rects when they share the active texture or atlas texture.
- Unsupported sprite paths warn/fallback instead of crashing.
- Existing `visible` filtering and stable `layer` ordering remain intact.

Build/run verification:

- `cmake --build --preset build-debug` passed.

Acceptance check:

- No renderer backend multi-texture API introduced.
- No per-entity material system introduced.
- Runtime mesh submission remains single-mesh.
- Renderer backend responsibilities did not expand.

### 2026-06-19 - Planning analysis after Renderable2D

Status: Planning update completed.

Observed code state:

- `Engine::Runtime2D::Renderable2D { visible, layer }` exists in `Engine/Runtime2D/Types.h`.
- `Entity2D` owns `renderable`.
- `SceneLoader2D` parses `visible` and `layer` with error reporting for invalid values.
- `Assets/Scene/main.xscene2d` includes visible/layer examples.
- `RenderSync2D` filters hidden entities, stable-sorts visible entities by layer, and clears runtime mesh when no entities are visible.
- `SpriteRef2D::spritePath` is still parsed but does not yet drive per-entity rendered sprite output.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-06-19.

Decision:

- Next step should be a same-texture/same-atlas `SpriteRef2D` rendering MVP.
- Reason: `sprite="..."` is now the most important scene/render semantic gap. It is already exposed to content, so it should either work in a clearly defined subset or warn/fallback clearly.

Current next task:

- Treat explicit runtime UVs as final for runtime meshes.
- Resolve entity sprite paths to UV rects when they share the active texture/atlas.
- Generate per-entity UVs in `RenderSync2D`.
- Keep multi-texture batching out of scope.

### 2026-06-19 - Renderable2D visible/layer scene rendering (plan completion)

Status: Completed.

Files changed:

- Added engine runtime renderable data:
  - `Engine/Runtime2D/Types.h`
  - `Engine/Runtime2D/World2D.h`
- Updated scene data:
  - `XiE_Projects/test2Dgame/Assets/Scene/main.xscene2d`
- Updated scene parsing:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneLoader2D.cpp`
- Updated render sync:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/RenderSync2D.cpp`

Behavior change summary:

- Scene entities can now define `visible=true/false` and `layer=<int>`.
- Hidden entities are not submitted to runtime render mesh.
- Visible entities are stable-sorted by layer.
- If all entities are hidden, `ClearRuntimeMesh2D()` is called to avoid stale geometry.

Build/run verification:

- `cmake --build --preset build-debug` passed.

Acceptance check:

- No full ECS introduced.
- No editor UI introduced.
- No renderer backend API change introduced.
- RenderSync owns project-side visibility/layer render policy.

### 2026-06-02 - Planning analysis after SceneRuntime2D hot reload

Status: Planning update completed.

Observed code state:

- `SceneRuntime2D.*` exists and uses validate-then-apply hot reload for `Assets/Scene/main.xscene2d`.
- `Test2DGameApp::OnUpdate` now runs:
  - `AssetHotReload -> SceneHotReload -> Input -> Move -> Collision -> RenderSync`
- Scene reload preserves the live world when parsing fails.
- `SceneLoader2D` parses `sprite="..."`, but `RenderSync2D` currently does not use per-entity `SpriteRef2D` for rendering.
- `RenderSync2D` currently batches every entity in world order and does not support visibility or layer ordering.
- `Renderer2DFeature` and `IRuntimeRender2D` are still single-runtime-mesh oriented, so full per-entity sprite/material rendering would be a larger renderer-facing change.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-06-02.

Decision:

- Next step should be `Renderable2D { visible, layer }`.
- Reason: it makes scene hot reload more useful immediately, gives the future editor visible object controls, and avoids prematurely taking on multi-sprite/material batching.
- Do not implement full per-entity sprite rendering yet.

Current next task:

- Add `Renderable2D` data to `Engine/Runtime2D`.
- Parse `visible` and `layer` in `SceneLoader2D`.
- Make `RenderSync2D` skip invisible entities and stable-sort visible entities by layer.
- Preserve existing scene compatibility and update-loop ordering.

### 2026-06-01 - SceneRuntime2D hot reload wrapper (plan completion)

Status: Completed.

Files changed:

- Added project-side scene runtime wrapper:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneRuntime2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneRuntime2D.cpp`
- Updated app lifecycle wiring:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.h`
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`

Behavior change summary:

- Startup path:
  - `m_SceneRuntime.Initialize(m_World)` loads `Assets/Scene/main.xscene2d`.
  - If startup scene load fails, app logs warning and uses `SpawnFallbackScene`.
- Runtime path:
  - `TickHotReload` tracks `last_write_time` for `main.xscene2d`.
  - On file change, it loads into temporary `World2D`.
  - Live world is replaced only when parsing succeeds.
  - On parse failure, current live world is preserved.
  - If scene file goes missing at runtime, warning is emitted once per missing period.
- Update order is now explicitly:
  - `AssetHotReload -> SceneHotReload -> Input -> Move -> Collision -> RenderSync`

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).
- Touch/edit scene reload smoke test passed (`ALIVE_AFTER_SCENE_EDIT=True`).
- Invalid scene edit recovery smoke test passed (`ALIVE_AFTER_INVALID_SCENE_EDIT=True`).

Acceptance check:

- Startup still loads from `Assets/Scene/main.xscene2d`.
- Runtime scene edits are detected without restart.
- Successful reload replaces live `World2D`.
- Failed reload preserves previous live `World2D`.
- Startup still falls back when initial scene is missing/invalid.
- Renderer boundaries unchanged; no ECS/editor UI introduced.

Next recommended plan:

1. Add project-side input-config hot reload (`Config/Input.toml`) with the same “validate-then-apply” policy.
2. Add minimal scene hot-reload diagnostics counters (reload attempts/success/failure) for quick iteration feedback.
3. If scene format stabilizes, evaluate a dedicated scene asset record path alongside `AssetRuntime2D`.

### 2026-05-31 - Planning analysis after main.xscene2d startup loading

Status: Planning update completed.

Observed code state:

- `Assets/Scene/main.xscene2d` exists and contains player/obstacle placement.
- `SceneLoader2D.*` parses the minimal line-based scene format and writes entities into `World2D`.
- `World2D::Clear()` exists and resets entities, player id, and next id.
- `Test2DGameApp::OnInit` loads `Assets/Scene/main.xscene2d` as primary path.
- `SpawnFallbackScene` remains available for startup recoverability.
- Scene loading currently happens only at startup.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-05-31.

Decision:

- Next step should be project-side scene hot reload.
- Reason: scene data is now editable, but still requires restart to see changes. Hot reload directly improves the solo-author iteration loop and previews how a future editor will write into the runtime.

Current next task:

- Add `SceneRuntime2D.h/.cpp`.
- Track `Assets/Scene/main.xscene2d` last write time.
- Reload into a temporary `World2D`.
- Replace live world only when parsing succeeds.
- Preserve previous live world on reload failure.
- Keep renderer and engine boundaries unchanged.

### 2026-05-31 - main.xscene2d loader + fallback spawn (plan completion)

Status: Completed.

Files changed:

- Added project scene asset:
  - `XiE_Projects/test2Dgame/Assets/Scene/main.xscene2d`
- Added project-side scene loader:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneLoader2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneLoader2D.cpp`
- Extended engine runtime world utility:
  - `Engine/Runtime2D/World2D.h`
  - `Engine/Runtime2D/World2D.cpp`
  - Added `World2D::Clear()` for world replacement before loading.
- Updated app initialization flow:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`
  - `OnInit` now loads `Assets/Scene/main.xscene2d` as primary startup path.
  - If loading fails, it logs warning and falls back to previous hard-coded player/obstacle spawn.

Behavior change summary:

- Initial entity placement is now content-driven from `Assets/Scene/main.xscene2d`.
- Hard-coded spawn is retained only as startup fallback path for recoverability.
- Update loop order remains unchanged:
  - `HotReload -> Input -> Move -> Collision -> RenderSync`

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).
- Missing-scene fallback smoke test passed (`ALIVE_WITHOUT_SCENE=True`).

Acceptance check:

- Primary startup path is no longer hard-coded spawn in `OnInit`.
- Startup succeeds when scene file is missing/invalid via fallback spawn.
- No renderer responsibilities expanded.
- No full ECS or editor UI introduced.

Next recommended plan:

1. Add optional runtime hot-reload for `Assets/Scene/main.xscene2d` (project-side) to support fast scene iteration without restart.
2. Add minimal scene validation warnings (for duplicate names, non-positive collider size) while keeping permissive defaults.
3. If format stabilizes after real use, evaluate promoting scene parsing/asset handling into an engine-level asset module.

### 2026-05-31 - Planning analysis after Input.toml-driven input

Status: Planning update completed.

Observed code state:

- `InputConfig2D.*` exists and performs small project-side key/value parsing.
- `InputSystem2D` loads configurable movement bindings and keeps safe GLFW defaults.
- `Config/Input.toml` contains movement bindings and move speed.
- `Test2DGameApp::OnInit` still hard-codes the initial player and obstacle entities.
- There is currently no `Assets/Scene/` directory or scene/level file.
- `Engine/Runtime2D` owns generic runtime services, but there is no scene serialization/loading path yet.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-05-31.

Decision:

- Next step should be a project-side minimal 2D scene file + loader.
- Reason: moving spawn positions and obstacle setup out of C++ gives a larger single-author workflow improvement than further input polishing.
- Keep this project-side first so the format can evolve from real usage before being promoted to engine-level scene/asset infrastructure.

Current next task:

- Add `Assets/Scene/main.xscene2d`.
- Add `SceneLoader2D.h/.cpp` under project `Runtime2D`.
- Load the scene in `Test2DGameApp::OnInit`.
- Fall back to the current hard-coded player + obstacle if loading fails.
- Keep update-loop order and renderer boundaries unchanged.

### 2026-05-31 - Input.toml-driven InputSystem2D (plan completion)

Status: Completed.

Files changed:

- Updated input config schema:
  - `XiE_Projects/test2Dgame/Config/Input.toml`
- Added small project-side input config parser:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/InputConfig2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/InputConfig2D.cpp`
- Updated project input system:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/InputSystem2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/InputSystem2D.cpp`
- Updated app initialization to load config:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`

Behavior change summary:

- `InputSystem2D` now loads movement bindings and move speed from `Config/Input.toml`.
- Supports minimal key/value parsing for this project (`key = "value"` and `key = number`).
- Supports primary + `_alt` directional bindings and `move_speed`.
- If config file is missing or invalid, startup continues with safe defaults and warning logs.
- Update loop ordering remains unchanged:
  - `HotReload -> Input -> Move -> Collision -> RenderSync`

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).
- Missing-config fallback smoke test passed (`ALIVE_WITHOUT_CONFIG=True`).

Acceptance check:

- Input mapping source of truth is no longer hard-coded to fixed WASD/arrows only.
- Arrow-key fallback is represented explicitly by `_alt` config keys.
- No renderer responsibilities expanded.
- No engine-level GLFW-dependent input system was introduced.

Next recommended plan:

1. Add optional runtime input-config hot reload (project-side) so rebinding can be tested without restart.
2. Introduce a small action-state struct in project code (`move_x`, `move_y`) to separate raw key polling from movement intent.
3. After action-state shape stabilizes, evaluate an engine-agnostic input utility layer without moving GLFW policy into `Engine`.

### 2026-05-31 - Planning analysis after MovementSystem2D extraction

Status: Planning update completed.

Observed code state:

- `Engine/Runtime2D/MovementSystem2D.*` exists and contains only generic velocity integration.
- `Test2DGameApp::OnUpdate` now calls:
  - `m_InputSystem.Tick(context.WindowHandle, m_World);`
  - `m_MovementSystem.Integrate(m_World, dt);`
  - `m_CollisionSystem.Solve(m_World);`
  - `m_RenderSync.Sync(...)`
- The minimal loop remains structurally clean.
- `XiE_Projects/test2Dgame/Config/Input.toml` exists and is copied by `Engine/Launch/CMakeLists.txt`.
- `InputSystem2D` still hard-codes GLFW key mappings in C++.
- No existing TOML/config parser was found in the engine or project code.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-05-31.

Decision:

- Next step should be project-side configurable input bindings.
- Do not move `InputSystem2D` into `Engine/Runtime2D` yet.
- Reason: current input code mixes low-level GLFW polling, binding data, and gameplay policy. Making bindings config-driven reduces solo-author iteration cost without prematurely designing a full engine input abstraction.

Current next task:

- Update `Config/Input.toml` with complete movement bindings.
- Add small project-side config parsing/key mapping for `InputSystem2D`.
- Preserve safe defaults if config is missing or invalid.
- Keep engine and renderer boundaries unchanged.

### 2026-05-31 - MovementSystem2D extraction (plan completion)

Status: Completed.

Files changed:

- Added engine Runtime2D movement service:
  - `Engine/Runtime2D/MovementSystem2D.h`
  - `Engine/Runtime2D/MovementSystem2D.cpp`
- Added project Runtime2D bridge alias:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/MovementSystem2D.h`
- Updated game app usage:
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.h`
  - `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`

Behavior change summary:

- Replaced inline movement integration loop in `Test2DGameApp::OnUpdate` with:
  - `m_MovementSystem.Integrate(m_World, dt);`
- Kept update order unchanged:
  - `HotReload -> Input -> Move -> Collision -> RenderSync`
- `InputSystem2D` remains project-owned in this step.

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).

Acceptance check:

- Movement is now a named engine-owned Runtime2D service.
- Project still runs the same minimal loop ordering.
- No renderer responsibilities expanded.

Next recommended plan:

1. Keep `InputSystem2D` project-owned until input mapping is split into:
   - low-level key/action state
   - configurable binding layer
   - gameplay-side velocity target selection
2. After the split design is stable, extract engine-side input state/binding utilities without hard-coding project player policy.

### 2026-05-31 - Planning analysis after Runtime2D world/collision extraction

Status: Planning update completed.

Observed code state:

- `Engine/Runtime2D/Types.h` owns reusable 2D data types.
- `Engine/Runtime2D/World2D.*` owns entity list, id allocation, lookup, and player id storage.
- `Engine/Runtime2D/CollisionSystem2D.*` owns dynamic-vs-static AABB separation.
- Project-side `World2D.h` and `CollisionSystem2D.h` are now alias bridges.
- `Test2DGameApp::OnUpdate` still contains inline movement integration.
- `InputSystem2D` is still project-owned and directly polls GLFW keys.
- `Config/Input.toml` exists, but current `InputSystem2D` does not consume it yet.

Build verification during planning:

- `cmake --build --preset build-debug` passed on 2026-05-31.

Decision:

- Next extraction should be `MovementSystem2D`, not `InputSystem2D`.
- Reason: movement integration is pure Runtime2D simulation; input currently mixes GLFW, key mapping, and player-specific gameplay policy.

Current next task:

- Add `Engine/Runtime2D/MovementSystem2D.h/.cpp`.
- Add a project alias bridge if useful.
- Replace inline movement integration in `Test2DGameApp::OnUpdate`.
- Keep minimal loop behavior unchanged.

### 2026-05-31 - Runtime2D world/collision extraction (plan completion)

Status: Completed.

Files changed:

- Added engine Runtime2D runtime services:
  - `Engine/Runtime2D/World2D.h`
  - `Engine/Runtime2D/World2D.cpp`
  - `Engine/Runtime2D/CollisionSystem2D.h`
  - `Engine/Runtime2D/CollisionSystem2D.cpp`
- Updated project Runtime2D bridge headers:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/World2D.h`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/CollisionSystem2D.h`
- Removed duplicate project-side implementations:
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/World2D.cpp`
  - `XiE_Projects/test2Dgame/Source/App/Runtime2D/CollisionSystem2D.cpp`

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).
- Minimal loop order remains unchanged:
  - `HotReload -> Input -> Move -> Collision -> RenderSync -> Render`

Acceptance check:

- Existing project Runtime2D files no longer define duplicate reusable component structs.
- `World2D` storage and `CollisionSystem2D` solving are now engine-owned reusable Runtime2D services.
- Renderer boundaries unchanged.

Next recommended extraction:

1. Move `InputSystem2D` to `Engine/Runtime2D/InputSystem2D` (keep project-specific input mapping/config in project).
2. Keep `AssetRuntime2D` and `RenderSync2D` in project for now, then evaluate an engine-level render-sync adapter only after editor/runtime scene representation is clearer.

### 2026-05-31 - Runtime2D data type extraction (first step)

Status: Completed (small-scope extraction only).

Files changed:

- Added `Engine/Runtime2D/Types.h`
  - Introduced engine-owned reusable data-only types:
    - `Vec2`
    - `Transform2D`
    - `Velocity2D`
    - `Collider2D`
    - `SpriteRef2D`
    - `EntityId` / `kInvalidEntityId`
- Updated `XiE_Projects/test2Dgame/Source/App/Runtime2D/World2D.h`
  - Removed duplicate local struct definitions.
  - Switched to aliases of engine-owned Runtime2D types.

Build/run verification:

- `cmake --build --preset build-debug` passed.
- `xie_launch.exe` short runtime smoke test passed (`ALIVE_AFTER_5S=True`).
- Existing minimal loop structure in `Test2DGameApp::OnUpdate` unchanged:
  - `HotReload -> Input -> Move -> Collision -> RenderSync -> Render`

Scope check:

- No ECS introduced.
- No renderer responsibilities expanded.
- No gameplay logic moved into renderer.
- Extraction limited to reusable data vocabulary.

Next recommended extraction:

1. Move `World2D` container (entity list + id/player management) into `Engine/Runtime2D` as a generic runtime world, while keeping test2Dgame-specific entity spawn/setup in project code.
2. After world extraction is stable, evaluate moving `CollisionSystem2D` (AABB separation only) to engine-level Runtime2D service.
