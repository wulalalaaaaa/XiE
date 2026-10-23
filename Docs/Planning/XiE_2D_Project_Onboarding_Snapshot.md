# XiE 2D Project Onboarding Snapshot

Date: 2026-07-08
Audience: A developer who has never seen this repository.
Scope: Current XiE engine architecture, 2D runtime/editor-readiness status, sample project, and known gaps.

## 1. Project North Star

XiE targets solo or small-team all-round creators.

The 2D engine and editor direction is not "build a Unity clone first". The current strategy is to build a small, coherent, hot-reloadable 2D game loop that lets one person move between code, scene layout, sprites, collision tuning, camera behavior, and debugging with as little context switching as possible.

Engineering priorities:

1. Reduce context switching for one full-stack creator.
2. Keep iteration fast and visible.
3. Preserve runtime stability and recoverability.
4. Keep module boundaries clear.
5. Add feature breadth only after the loop is coherent.

## 2. Repository Shape

Top-level structure:

- `Engine`: reusable engine code.
- `Engine/Core`: application lifecycle, game app interface, frame context, logging.
- `Engine/Launch`: shared executable entry point and project bootstrap.
- `Engine/Renderer`: renderer facade, OpenGL backend, 2D/3D render features, camera, mesh loading.
- `Engine/Assets`: small engine asset loaders and `AssetManager`.
- `Engine/Runtime2D`: reusable 2D runtime data and simple systems.
- `Engine/Story`: experimental story/dialogue runtime module.
- `Engine/External`: local GLFW/GLAD sources.
- `Engine/ThirdLibraries`: third-party fallback libraries such as `spdlog`.
- `XiE_Projects/test2Dgame`: current sample 2D game project.
- `Docs`: project memory, verification notes, architecture handoff, and this onboarding snapshot.

Build targets:

- `xie_engine`: shared engine library.
- `xie_game_test2d`: static sample project library.
- `xie_launch`: runnable app that links engine + sample game.
- `xie_story_tests`: standalone story runtime/catalog self-test executable.

The root `CMakeLists.txt` adds `Engine`, `XiE_Projects/test2Dgame`, and `Engine/Launch`. Visual Studio startup target is `xie_launch`.

## 3. Runtime Ownership Model

The current project has a deliberate split:

- `Engine` owns reusable infrastructure and stable runtime vocabulary.
- `Renderer` owns GPU/backend details, projection math, asset-to-draw submission, and render features.
- `XiE_Projects/test2Dgame` owns current game policy, project-side file formats, scene reload policy, input mapping, camera follow policy, and render-sync policy while those are still being proven.

Do not move systems into `Engine` just because they exist. Promote only after the behavior has stabilized through the sample project.

## 4. Main Application Flow

The entry path is:

```text
xie_launch
  -> Engine/Launch/EntryPoint.cpp
  -> Engine/Launch/GameBootstrap.cpp
  -> project CreateGameApplication()
  -> Test2DGameApp
```

`Engine::Application` owns the window, renderer, frame context, and lifecycle calls into `IGameApp`.

`Engine::FrameContext` currently exposes:

- `GLFWwindow* WindowHandle`
- `IRuntimeRender2D* RuntimeRender2D`

`Engine::IRuntimeRender2D` currently exposes:

- `SubmitRuntimeMesh2D(...)`
- `ClearRuntimeMesh2D()`
- `SetCamera2D(float centerX, float centerY, float zoom)`

## 5. Current 2D Runtime Loop

The current effective update order in `Test2DGameApp::OnUpdate` is:

```text
AssetHotReload
-> SceneHotReload
-> EnsureSpriteRefs
-> Input
-> StoryInput
-> Movement
-> Collision
-> Camera
-> RenderSync
-> StoryEventDrain
-> Render
```

The 2D-critical path is:

```text
AssetHotReload
-> SceneHotReload
-> EnsureSpriteRefs
-> Input
-> Movement
-> Collision
-> Camera
-> RenderSync
-> Render
```

Story input/event draining is an additional prototype layered into the sample app. It is not required for the 2D render/movement/collision loop.

## 6. Engine Runtime2D

`Engine/Runtime2D` contains stable, reusable 2D runtime pieces:

- `Vec2`
- `Transform2D`
- `Velocity2D`
- `Collider2D`
- `SpriteRef2D`
- `Renderable2D`
- `Entity2D`
- `World2D`
- `MovementSystem2D`
- `CollisionSystem2D`

`Entity2D` currently contains:

- `id`
- `transform`
- `velocity`
- `collider`
- `sprite`
- `renderable`

`World2D` currently provides:

- entity creation
- entity lookup by id
- entity vector access
- world clear
- one player id slot

`MovementSystem2D` performs generic velocity integration:

```text
position += velocity * dt
```

`CollisionSystem2D` performs simple dynamic-vs-static AABB separation. This is intentionally not a full physics engine.

## 7. Project Runtime2D

`XiE_Projects/test2Dgame/Source/App/Runtime2D` contains project-side systems that are still being proven:

- `World2D.h`: aliases engine `Runtime2D` world/types into the project namespace.
- `MovementSystem2D.h`: aliases engine movement system.
- `CollisionSystem2D.h`: aliases engine collision system.
- `InputConfig2D.*`: small project-side input config parser.
- `InputSystem2D.*`: GLFW polling and movement velocity policy.
- `SceneLoader2D.*`: project-side line-based `.xscene2d` loader.
- `SceneRuntime2D.*`: validate-then-apply hot reload wrapper for the scene file.
- `AssetRuntime2D.*`: project-side asset hot reload and sprite/atlas reference tracking.
- `RenderSync2D.*`: converts `World2D` entities into one submitted runtime mesh.
- `CameraRuntime2D.*`: project-side camera follow policy that drives `IRuntimeRender2D::SetCamera2D`.

## 8. Scene Format

The current scene file is:

```text
XiE_Projects/test2Dgame/Assets/Scene/main.xscene2d
```

Current format:

```text
xie_scene2d 1
entity name=player x=200 y=200 half_x=24 half_y=24 static=false sprite="Assets/Sprite/main.xsprite" player=true visible=true layer=10
entity name=obstacle x=480 y=260 half_x=64 half_y=64 static=true sprite="Assets/Sprite/checker_full.xsprite" visible=true layer=0
```

Supported entity fields:

- `name`: parsed but currently ignored after parse.
- `x`, `y`: world position.
- `half_x`, `half_y`: collider/render half extents.
- `static`: collision static flag.
- `sprite`: sprite descriptor path.
- `player`: marks the single player entity.
- `visible`: render visibility flag.
- `layer`: stable draw order key.

Scene loading rules:

- Header must be `xie_scene2d 1`.
- Scene must contain at least one entity.
- Scene must contain exactly one `player=true` entity.
- Multiple player entities are rejected.
- Invalid numeric/bool fields reject the scene load.
- On startup failure, the app uses a hard-coded fallback scene.
- On hot-reload failure, the current live world is preserved.

Current known scene limitations:

- `name` is not stored in `Entity2D`.
- No duplicate-name diagnostics.
- No non-positive collider validation.
- No scene-level camera settings yet.
- No hierarchy, prefab, script binding, animation binding, or component blocks.

## 9. Asset Pipeline

Current sample assets live under:

```text
XiE_Projects/test2Dgame/Assets
```

Important asset paths:

- `Assets/Mesh/main.xmesh`
- `Assets/Texture/main.xtexture`
- `Assets/Texture/checker.png`
- `Assets/Texture/checker.jpg`
- `Assets/Material/main.xmat`
- `Assets/Atlas/main.xatlas`
- `Assets/Sprite/main.xsprite`
- `Assets/Sprite/checker_full.xsprite`
- `Assets/Scene/main.xscene2d`
- `Assets/Story/.../*.story`

`Engine/Assets` provides loaders for:

- mesh-related renderer assets through `Renderer/MeshAsset.*`
- `TextureAsset`
- `MaterialAsset`
- `SpriteAsset`
- `AtlasAsset`
- `AssetManager`

`AssetRuntime2D` currently:

- sets project asset root to `Assets`
- loads default mesh/material/sprite/texture
- resolves atlas and texture references
- hot-reloads tracked mesh/material/sprite/atlas/texture files
- tracks sprite paths referenced by scene entities
- exposes an `AssetRuntime2DSnapshot` to `RenderSync2D`

## 10. Rendering Path

The renderer shape is:

```text
Engine::Renderer
  -> BasicRenderer
  -> Renderer2DFeature
  -> OpenGLBackend
```

`Renderer2DFeature` owns:

- active mesh upload
- active material/texture application
- runtime mesh storage
- `Camera2D`
- canvas defaults
- runtime camera setter

Runtime rendering uses one submitted mesh:

```text
World2D entities
  -> RenderSync2D builds combined vertices/uvs/indices
  -> IRuntimeRender2D::SubmitRuntimeMesh2D
  -> BasicRenderer
  -> Renderer2DFeature::SubmitRuntimeMesh
  -> backend upload/draw
```

Current render semantics:

- Invisible entities are skipped.
- Visible entities are stable-sorted by `renderable.layer`.
- Per-entity sprite path affects UVs when supported.
- Unsupported sprite cases warn and fallback instead of crashing.
- If there are no visible entities, runtime mesh is cleared.
- Runtime mesh UVs are treated as final by `Renderer2DFeature`.

Current render limitations:

- One active texture/material path.
- No multi-texture batching.
- No per-entity material.
- No z-depth sorting beyond 2D `layer`.
- No editor viewport camera.
- No render targets or post-processing.

## 11. Sprite Rendering

`SpriteRef2D::spritePath` is now meaningful.

Current supported subset:

- Entity sprite paths can point to different `.xsprite` files.
- Those sprites can use explicit UV rects or atlas regions.
- They are supported when they resolve to the same active texture or active atlas texture.
- Different texture cases are not batched yet; they warn and fallback.

This is a deliberate MVP, not a full renderer material system.

## 12. Camera/View

Camera runtime control MVP is implemented.

Public runtime bridge:

```cpp
IRuntimeRender2D::SetCamera2D(float centerX, float centerY, float zoom)
```

Forwarding path:

```text
Test2D::CameraRuntime2D
  -> IRuntimeRender2D::SetCamera2D
  -> BasicRenderer::SetCamera2D
  -> Renderer2DFeature::SetCamera
  -> Camera2D::SetPosition / SetZoom
```

`CameraRuntime2D` current behavior:

- defaults to `follow_player=true`
- defaults to `zoom=1.0`
- fallback center is `(640, 360)`
- follows `World2D::GetPlayer()` when present
- invalid zoom falls back to `1.0`
- missing player warns and uses fallback center

Coordinate note:

- Runtime/game coordinates currently behave like top-left/y-down 2D canvas coordinates.
- `Renderer2DFeature::SetCamera` converts runtime center semantics into the current `Camera2D` position semantics when `Canvas2DDesc::originAtCenter == false`.

Current camera limitations:

- Camera settings are not yet content-driven through scene/config.
- No smoothing/dead-zone.
- No bounds.
- No shake.
- No editor viewport camera.
- No camera stacks.

## 13. Input

Input config:

```text
XiE_Projects/test2Dgame/Config/Input.toml
```

Current supported fields:

- `move_left`
- `move_right`
- `move_up`
- `move_down`
- `move_left_alt`
- `move_right_alt`
- `move_up_alt`
- `move_down_alt`
- `move_speed`

Default mapping:

- WASD
- Arrow keys as alternatives
- movement speed `260`

Current input behavior:

- `InputSystem2D` polls GLFW directly.
- It finds the player entity.
- It writes normalized movement velocity to the player.
- If config is missing/invalid, safe defaults remain.

Current input limitations:

- Config is loaded at startup only.
- There is no action-state abstraction yet.
- Engine-level input abstraction has not been introduced.
- Story demo keys `F5/F6/F7` are handled directly in `Test2DGameApp`.

## 14. Collision/Physics

Current collision is enough for a minimal movement loop:

- dynamic-vs-static AABB separation
- uses collider half extents
- resolves overlap by minimum penetration axis
- static entities do not move

Current physics limitations:

- No collision events.
- No triggers.
- No layers/masks.
- No swept collision.
- No raycasts or spatial queries.
- No rigidbody forces, mass, restitution, friction, or continuous collision.

## 15. Story Runtime Prototype

`Engine/Story` is a separate experimental module currently integrated into `test2Dgame`.

It is not required for the 2D movement/render/collision loop, but it matters for current project state.

Engine story pieces:

- `StoryTypes`: story/session/command/event/result data.
- `IStoryCatalog`: read-only story content lookup interface.
- `StoryFileCatalog`: loads story files from a directory tree.
- `StoryParser`: parses `.story` files into ordered lines.
- `IStoryRuntime`: runtime interface.
- `StoryRuntime`: authority/replica session runtime.
- `IStoryTransport`: command/state transport abstraction.
- `LoopbackStoryTransport`: in-process transport for local authority/replica tests.

Sample project story pieces:

- `Assets/Story/示例/初次见面/*.story`
- `Source/Story/StorySelfTest.*`
- `Tests/StoryTests.cpp`

Runtime demo controls:

- `F5`: start first story.
- `F6`: advance current story.
- `F7`: abort current story.

Story runtime semantics:

- Authority applies commands.
- Replicas synchronize from authority state.
- Commands include expected revision and content version.
- Rejected cases include missing session, content mismatch, revision mismatch, unauthorized peer, duplicate command, and already-ended story.
- Late replica connection replays latest state.

Current story limitations:

- No UI presentation layer.
- No save/load persistence.
- No network transport.
- No branching choice UI.
- No localization pipeline.
- No editor tooling.

## 16. Build And Verification

Known build command:

```powershell
cmake --build --preset build-debug
```

Previously verified milestones:

- configure/build passed during minimal loop work
- short runtime smoke test passed
- scene hot-reload smoke test passed
- invalid scene reload preserved live world
- missing config/scene fallback smoke tests passed
- per-entity sprite rendering MVP build passed
- camera runtime control MVP was reported implemented by the implementation session

Current verification on 2026-07-08:

- `cmake --build --preset build-debug` passed.
- `xie_story_tests.exe Assets/Story` passed.
- Story test output: `checks=33 message=All story checks passed`.

Current note:

- This snapshot documents the code state as inspected on 2026-07-08.
- It does not imply every desktop/manual interaction has been re-smoked on this date unless a newer verification note says so.

## 17. Current Completion Level

2D runtime coherence status: mostly complete for a minimal game loop.

Completed enough for MVP iteration:

- shared launch/game bootstrap
- project-owned game app
- reusable 2D world/types
- movement system
- simple AABB collision
- input config at startup
- scene file startup load
- validate-then-apply scene hot reload
- asset hot reload
- renderable `visible`
- renderable `layer`
- per-entity same-texture/same-atlas sprite UV rendering
- one-mesh runtime render submission
- game runtime camera control
- player-follow camera policy
- fallback/warning behavior for several failure cases

Not complete enough for a real 2D editor workflow:

- entity names are not retained
- scene validation is still minimal
- scene reload diagnostics are not surfaced as structured counters
- camera settings are not content-authored
- collision has no events/layers/triggers
- no animation system
- no editor UI
- no project template generator
- no packaging workflow

## 18. Practical "Can I Make A 2D Game Yet?" Answer

You can currently make a very small 2D prototype with:

- a scene file with player and obstacle entities
- configurable movement speed and keys
- static collision obstacles
- sprite assignment from same texture/atlas
- visibility/layer control
- player-follow camera
- hot reload for scene/assets

You cannot yet comfortably make a full 2D game because the missing systems are still core:

- animation
- collision events/triggers
- multiple scenes or scene transitions
- UI
- audio
- save data
- editor authoring
- packaging
- robust content validation

## 19. Most Important Next Work

Recommended next milestone:

```text
Scene Authoring And Diagnostics MVP
```

Reason:

The runtime loop is now coherent enough that the next blocker is authoring quality. A future editor needs stable entity identity, validation, and scene-level metadata before UI work is useful.

Suggested scope:

- Store `name` in `Entity2D` or a lightweight metadata component.
- Detect duplicate entity names during scene load.
- Validate non-positive collider sizes.
- Add scene-level camera defaults such as follow player, zoom, fallback center.
- Expose scene reload diagnostics such as attempts/success/failure/last error.
- Keep format simple and line-based unless editor needs force a richer format.

Non-goals for the next milestone:

- no full ECS
- no editor UI yet
- no prefab system
- no scripting
- no YAML/JSON migration unless the current format blocks progress

## 20. First Files To Read

Read these in order:

1. `Docs/XiE_2D_Project_Onboarding_Snapshot.md`
2. `Docs/2D_Engine_Implementation_Handoff.md`
3. `Docs/2D_Engine_Completion_Gap_Analysis.md`
4. `Engine/Core/GameApp.h`
5. `Engine/Core/Application.cpp`
6. `Engine/Launch/GameBootstrap.cpp`
7. `XiE_Projects/test2Dgame/Source/App/Test2DGameApp.cpp`
8. `Engine/Runtime2D/Types.h`
9. `Engine/Runtime2D/World2D.h`
10. `XiE_Projects/test2Dgame/Source/App/Runtime2D/SceneLoader2D.cpp`
11. `XiE_Projects/test2Dgame/Source/App/Runtime2D/RenderSync2D.cpp`
12. `XiE_Projects/test2Dgame/Source/App/Runtime2D/CameraRuntime2D.cpp`
