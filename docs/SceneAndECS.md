# Scene, ECS and systems

A scene is an [EnTT](https://github.com/skypjack/entt) registry. Entities are IDs, components are plain data, and behaviour lives in **systems**: functions that run over every entity with a given set of components, in a fixed order, each frame.

"System" in Hydro always means an ECS system like this. Engine-wide services with a lifetime (`Renderer`, `Log`, `Time`, ...) are **subsystems**; see [Subsystems](Subsystems.md). Engine systems take `(Scene&, const SystemContext&)`; the context carries the frame's delta time (`Time::GetDeltaTime()`), the fixed step, the frame index and whether the game is playing (see [System context and stages](#system-context-and-stages)).

## The pieces

| Piece | Where | What it is |
| --- | --- | --- |
| `Scene` | `Scene/Scene.h` | Owns the `entt::registry`; creates and destroys entities (`CreateEntity`, `DestroyEntity`); runs the scene's systems in `OnUpdateRuntime` / `OnUpdateEditor`; knows its viewport size (`OnViewportResize`) and primary camera (`GetPrimaryCameraEntity`) |
| `Entity` | `Scene/Entity.h` | A handle (`entt::entity` + `Scene*`). Cheap to copy, store it by value |
| Components | `Scene/Components/Components.h` (`NativeScriptComponent`: `Scene/ScriptableEntity.h`) | Plain structs: `TagComponent`, `IDComponent`, `TransformComponent`, `SpriteRendererComponent`, `SpriteAnimatorComponent`, `CameraComponent`, `Light2DComponent` (Point, Global, Spot), `DirectionalLightComponent` (the sun of forward lighting), `MeshComponent`, `Shadow2DComponent`, `Rigidbody2DComponent`, `BoxCollider2DComponent`, `CircleCollider2DComponent`, `ParticleEmitter2DComponent` (settings only; its particles are in the `ParticleScene`), `NativeScriptComponent` |
| Systems | `Scene::OnUpdateRuntime` / `OnUpdateEditor`, `SceneRenderer`, application layers | Functions over registry views |
| `SceneRenderer` | `Renderer/SceneRenderer.h` | Submits a scene to `Renderer2D`, into a `RenderView`, through a camera: `RenderEditor(view, editorCamera, overlay)` or `RenderRuntime(view)` (primary camera) |
| Scripts | `Scene/ScriptableEntity.h`, `Scene/ScriptRegistry.h` | Per-entity C++ behaviour, run by the script system; registered by name so they can be saved and added in the editor |
| Serialization | `Scene/SceneSerializer.h`, `Scene/PrefabSerializer.h`, `Scene/EntitySerializer.h` | Saves scenes (`.hyscene`) and prefabs (`.hyprefab`) as YAML |

Every entity created with `Scene::CreateEntity` gets a `TagComponent` (its name) and an `IDComponent` (a 64-bit `UUID` that stays the same across saves).

## Components are data

- Public fields with defaults, plus small pure helpers (`TransformComponent::GetTransform()`). No virtual functions, and no update logic.
- No raw pointers or references to other components or entities. Refer to other entities by `Entity` or `UUID`.
- A shared engine resource is fine: `SpriteRendererComponent::Texture` was a `Ref<Texture2D>` and is now an `AssetHandle` (copied with the component, so the play copy refers to the same asset); the renderer resolves it with `AssetManager::GetLoadedAsset` ([Architecture](Architecture.md#asset-system)).
- Find an entity by its UUID with `Scene::GetEntityByUUID(uuid)`; it returns an empty `Entity` when there is none. `Entity::IsValid()` is true for an entity that exists and is not pending destroy; `HasComponent` on an empty or destroyed entity is false. `GetComponent` asserts (debug builds) that the entity is alive and has the component, and has a `const` overload.
- Creating an entity with a UUID that is already used (`CreateEntityWithUUID`) logs an error and gives the new entity a fresh UUID, so lookups stay unambiguous.
- Don't keep a `T&` from `GetComponent<T>()` across a call that adds or removes components. EnTT moves its storage, so the reference can dangle.

### Where runtime state lives (component state rule)

Agreed October 25, 2026 (particles design). A component's *settings* and its *runtime state* follow these rules, so there is one way to decide where a field goes:

1. **Saved settings** live in the component.
2. **Runtime state that is fixed-size, per entity and needs nothing outside the component** may live in the component, grouped in one `State` member (a plain struct named `<Component name without Component>State`). It is never saved, it is reset when the game starts (`Scene::OnRuntimeStart`), and only its system and its facade write it. *Example: `SpriteAnimatorComponent::State` (`SpriteAnimatorState`).*
3. **Runtime state that is variable-size, owned by a library or the GPU, or must exist apart from the entity** lives in a per-scene `...Scene` object owned by `Scene`, and then **all** of that component's runtime state goes there (one source of truth). The component holds settings only. *Examples: `PhysicsScene` (Box2D bodies); `ParticleScene` (particle buffers, planned, Roadmap V2).*
4. **Scripts and editor tools go through the facade** (`SpriteAnimator`, `Physics2D`, `Particles2D` planned), never through runtime fields. Editor windows may read settings directly; they read playback or simulation state through the facade.

`Scene::Copy` copies a rule-2 `State` along with the component (it is plain data); the reset in `OnRuntimeStart` makes the game start from the settings anyway.

## Systems are functions

A system takes the registry (or the scene) and does one job over a view:

```cpp
void UpdateSpin(entt::registry& registry, float deltaTime)
{
    auto view = registry.view<TransformComponent, SpinComponent>();
    for (auto entity : view)
    {
        auto [transform, spin] = view.get<TransformComponent, SpinComponent>(entity);
        transform.Rotation.z += spin.Speed * deltaTime;
    }
}
```

- Use `view<...>()`. Avoid owning `group<...>()`: only one owning group may claim a component type, so a second system that wants one asserts.
- Each system reads and writes only the components in its view.
- Systems don't call Vulkan. Render systems *submit* to the renderer (`Renderer2D::DrawQuad`), and `Renderer::RenderFrame` records the GPU work later.

## Frame order

A scene is updated one of two ways:

- `Scene::OnUpdateRuntime(dt)` — the game is running: builds a `SystemContext` (`IsPlaying = true`) and runs the stages below in order.
- `Scene::OnUpdateEditor(dt)` — editing: builds a context with `IsPlaying = false` and runs only the transform stage, no scripts or animation. Oxygen calls this one except in play mode (see [Play mode](#play-mode)).

| Order | System | Runs in | Reads → writes |
| --- | --- | --- | --- |
| 1 | `UpdateScripts` (runtime only) | `Scene::OnUpdateRuntime` | `NativeScriptComponent` → whatever scripts touch |
| 2 | Physics (runtime only) | `Scene::OnUpdateRuntime` | `PhysicsScene::Step`: kinematic bodies are carried to their transforms, fixed 60 Hz steps run, dynamic bodies' results are written to their transforms (see below) |
| 3 | `UpdateSpriteAnimations` (runtime only) | `Scene::OnUpdateRuntime`, after the scripts | `SpriteAnimatorComponent` → `SpriteRendererComponent` (`Texture`, `SpriteIndex`, `FlipX`) |
| 4 | `Scene::UpdateTransforms` | `OnUpdateEditor` and `OnUpdateRuntime` | None yet; transform propagation goes here |
| 5 | `UpdateParticles` (runtime only) | `Scene::OnUpdateRuntime`, after transforms | `TransformComponent` + `ParticleEmitter2DComponent` → the scene's `ParticleScene` (emits, simulates, retires particles); destroys finished `DestroyWhenFinished` entities after its loop |
| 6 | Application systems | The layer, after the scene update | Its own components. The editor's `UpdateGalaxy` reads `GalaxyStar` and writes `TransformComponent` + `SpriteRendererComponent` |
| 7 | Sprite, particle and light submission | `SceneRenderer::RenderEditor` / `RenderRuntime` | camera → `Renderer2D::BeginScene`; `TransformComponent` + `SpriteRendererComponent` → `Renderer2D::DrawQuad(transform, texture, tiling or sprite rect, colour)`, the quad turned for its `Facing` (Upright, Billboard) when the camera is tilted; `TransformComponent` + `Light2DComponent` → `Renderer2D::DrawLight` / `DrawGlobalLight`; `TransformComponent` + `Shadow2DComponent` → `Renderer2D::DrawShadow` (the view is then drawn lit, see [Architecture](Architecture.md#2d-lighting)). The scene's post-processing (`Scene::GetPostProcess`, exposure for HDR views) goes to `Renderer2D::SetPostProcess`. In forward-lit views lights, the main `DirectionalLightComponent` and the scene's environment become one `SceneLighting` (`Renderer2D::SetSceneLighting`, [Architecture](Architecture.md#forward-lighting)) and shadows are decals; with sun shadows, cut-out standing sprites also queue their shadow casters (`Renderer2D::BeginShadowCasters`, [Architecture](Architecture.md#sun-shadows)) and meshes pass `MeshComponent::CastShadows` |
| — | GPU | `Renderer::RenderFrame` (application loop) | Records and presents everything submitted |

New engine systems are a `(Scene&, const SystemContext&)` function called from a stage: in `Scene::OnUpdateEditor` as well when they must also run while editing, otherwise only in `Scene::OnUpdateRuntime`, in a deliberate position. Game-only systems return at once when `context.IsPlaying` is false. Gameplay systems belong to the application and run after the scene update.

## Cameras

A scene can be drawn through several cameras in one frame, each into its own `RenderView` (see [Architecture](Architecture.md#render-views)).

| Camera | Where | Used by |
| --- | --- | --- |
| `CameraComponent` (holds a `SceneCamera`) | `Scene/Components/Components.h`, `Renderer/SceneCamera.h` | The game: `SceneRenderer::RenderRuntime` draws through the first camera with `Primary` set |
| `EditorCamera` | `Renderer/EditorCamera.h` | Oxygen's **Editor** window: `SceneRenderer::RenderEditor(view, camera)`. Not part of the scene and not saved |

- **Data only.** `SceneCamera` is the projection settings (perspective FOV/near/far, orthographic size/near/far, default orthographic with 10 units visible vertically). The camera's position and rotation come from the entity's `TransformComponent` (scale is ignored); its aspect ratio comes from the scene's viewport, set by `Scene::OnViewportResize` (Oxygen's Game window calls it with its size). Nothing has to be kept in sync by hand.
- **One primary camera.** `Scene::GetPrimaryCameraEntity` returns the first `CameraComponent` with `Primary` set. The editor unticks the others when you tick one. Without a primary camera, `RenderRuntime` draws nothing and returns `false`.
- **Clip space is handled once**, in `Camera` (`Renderer/Camera.h`): every projection is built by `Camera::Perspective` / `Camera::Orthographic`, which use GLM's zero-to-one depth (`*_ZO`) and flip Y for Vulkan. World +Y is up on screen, geometry is counter-clockwise (`VulkanPipeline` front face), and the rendered image is shown without flipping its UVs. Never build a projection with `glm::perspective`/`glm::ortho` directly.
- **World units, not pixels.** Sprites are sized in world units (a default sprite is 1×1). The editor's galaxy fits the default 10-unit view.
- **Editor camera controls** (Oxygen's Editor window, **Camera** menu, and the **2D / 3D** switch next to it): **2D** is orthographic and looks straight down −Z (pan and zoom only); **3D** is perspective. The world is Z-up and the camera orbits like a turntable: yaw about world Z, pitch from straight down (0) past level (90) to 175 degrees, so the horizon never rolls. Alt + left drag orbits the focal point, middle drag pans, Alt + right drag or the wheel zooms, and holding the right button flies (mouse look from the camera's position; W/A/S/D along the view, Q/E down/up the world's Z, Shift x3, the wheel scales the speed while flying; `EditorCameraSettings::FlySpeed` m/s). Standard views: Top, Diorama (50 degrees, the field scenes' tilt), Front / Back / Left / Right (level, looking north / south / east / west); 3D goes back to its last angle after 2D. Flying reads the keys through `Input`, so in play mode W/A/S/D also reach the game while the right button is held over the Editor window. The **Game** window always shows the primary camera.

## Play mode

Oxygen edits one scene and plays a copy of it, so nothing that happens while playing changes what is saved.

| Button (top bar) | What happens |
| --- | --- |
| **Play** | `Scene::Copy(editorScene)` makes the runtime scene; it becomes the active scene. Each frame: `OnUpdateRuntime` (scripts + engine systems), then the application's systems |
| **Pause** / **Resume** | Stops/continues updating the runtime scene; it is still drawn (`OnUpdateEditor` runs instead) |
| **Step** (while paused) | Runs one runtime frame |
| **Stop** | Throws the runtime scene away (its scripts get `OnDestroy`); the editor scene becomes active again |

- `Scene::Copy` copies every entity made with `CreateEntity` (same UUIDs), the components in `CopyableComponents` (`Components.h`: add new copyable components there), the native script bindings (fresh instances on the first runtime update), the name, the viewport size and the scene's render settings. Raw registry entities without an `IDComponent` are not copied; an application copies its own (the editor copies its galaxy stars).
- The hierarchy, inspector and both windows show the active scene, so the runtime scene can be inspected and tweaked while playing; those changes are lost on Stop. Saving always writes the editor scene, and opening or creating a scene stops play first.
- The selected entity stays selected across Play and Stop: the editor finds it again in the new scene by its UUID (`Scene::GetEntityByUUID`), and clears the selection if it no longer exists (for example an entity spawned while playing, after Stop).
- Application systems are gameplay: Oxygen's galaxy only moves while playing.
- **Scene stack.** While playing, a script can switch scenes with `SceneTransition::Push("Name")` (`Hydro/Scene/SceneTransition.h`): the host loads `Assets/Scenes/Name.hyscene` on top of the running scene, which stops updating and drawing but stays exactly as it was (entities, scripts, physics bodies). `SceneTransition::Pop()` throws the top scene away and the one below carries on. Scripts only record the request; the host (`EditorLayer::ProcessSceneTransition`, called after the frame's update) does it, so the scene is never changed in the middle of its own update. Stop discards the whole stack. Data that has to cross scenes (a battle's result) lives in plain game state, like `BattleState`. A future standalone runtime does the same with its own scene list.

## Scripts

Scripts are the per-entity escape hatch. Write a system instead when the logic applies to many entities the same way.

```cpp
class PlayerController : public Hydro::ScriptableEntity
{
protected:
    void OnCreate() override {}
    void OnUpdate(float deltaTime) override
    {
        auto& transform = GetComponent<TransformComponent>();
        transform.Translation.x += 2.0f * deltaTime; // world units per second
    }
};

// Once, before any scene is loaded (Application::OnInit):
Hydro::ScriptRegistry::Register<PlayerController>("PlayerController");

// Then bind by type in code, or by name (what loading a scene and the editor do):
entity.AddComponent<NativeScriptComponent>().Bind<PlayerController>().Bind("Health");
```

- `NativeScriptComponent` is data: a list of scripts (name, factory, instance). An entity has one `NativeScriptComponent` but any number of scripts in it. Instances are created on the scene's next runtime update, live on the heap, and hold their `Entity` by value.
- **`ScriptRegistry`** maps a name to a script's factory. Scenes and prefabs save the names (`NativeScriptComponent: { Scripts: [Spin, Bob] }`) and loading binds them again by name. Register every script before a scene that uses it is loaded; names must stay stable once saved. A script bound in code but not registered still runs, but isn't saved. A saved name that isn't registered is kept (so the file saves back unchanged) but doesn't run, and the editor shows it in red.
- Remove scripts with `Scene::RemoveScript(entity, index)` / `Scene::RemoveScripts(entity)`, which call `OnDestroy` on running ones. Oxygen's inspector lists an entity's scripts, removes them, and adds registered ones (**Add Component > Script** or **Add Script**).
- **Scripts can change the scene.** From `OnCreate`/`OnUpdate` a script may create entities, add components, bind scripts, and destroy entities or remove scripts, including its own. The script stage (`UpdateScripts`) walks a snapshot of the entities that had scripts when it began, and two rules keep it safe ([#40](https://github.com/joelnl74/Hydro/issues/40)):
  - **Created things start next update.** An entity made during the stage exists at once (you can add components to it), but its scripts first run on the next update. A script bound to an existing entity does the same.
  - **Removal happens when the stage ends.** While the stage runs, `Scene::DestroyEntity`, `RemoveScript` and `RemoveScripts` only mark the entity or script: it stops running at once (no more `OnUpdate`), `Scene::IsPendingDestroy(entity)` is true, and the entity stays valid (`GetComponent` still works) until the stage ends. Then `OnDestroy` is called and it is removed. Calling them twice is harmless. Outside a stage (the editor's hierarchy and inspector) they act at once.
  - `OnDestroy` may itself destroy other entities or remove scripts; those are applied in the same round, after it. When a `Scene` is destroyed, such requests are dropped.
  - A script removed with `RemoveScripts` leaves an empty `NativeScriptComponent` behind once the stage ends (removing it outright would need a component removal while a stage runs).
  - `m_StageDepth` (private) is what the deferral checks; any stage that runs user code (physics callbacks, later) brackets its work with `Scene::BeginStage` / `EndStage`.
- `Scene::DestroyEntity` calls `OnDestroy` on the entity's scripts before removing it.

## Saving scenes and prefabs

Scenes and prefabs are YAML files written with yaml-cpp.

| File | Written by | Holds |
| --- | --- | --- |
| `.hyscene` | `SceneSerializer` | The scene name, its `RenderSettings` (only when it overrides the project's render features) and every entity that has an `IDComponent` |
| `.hyprefab` | `PrefabSerializer` | One entity and its components, to spawn copies of |

- **One place per component.** `EntitySerializer` writes and reads every component for both file types. A new component that should be saved goes into `EntitySerializer::Serialize` and `EntitySerializer::Deserialize`, and nowhere else.
- **Only entities made with `CreateEntity` are saved.** Raw registry entities without an `IDComponent`, such as the editor's galaxy stars, are runtime-only.
- **Render settings.** A scene can override the project's render features (`SceneRenderSettings`: `OverrideProject` + `RenderFeatureSettings`, edited in **Render Settings > Rendering (scene)**). Scenes that don't override write nothing, so older files and 2D scenes stay as they were. See [Render features](Architecture.md#render-features-and-the-render-graph).
- **UUIDs.** Loading a scene keeps the saved UUIDs. Instantiating a prefab gives each copy a new UUID, so copies never clash.
- **Prefabs as loaded assets (Oct 25, 2026).** `AssetManager::GetAsset<Prefab>(handle)` reads a `.hyprefab` once into a `Prefab` (`Scene/Prefab.h`: its name and the saved entity as YAML) and keeps it like a texture; it loads again when the file changes. `PrefabSerializer::Instantiate(scene, prefab)` makes a copy from a loaded `Prefab` without reading the file or logging; `Instantiate(scene, handle)` loads it first when needed. The path-based `Instantiate` / `Load` still read the file every time (the editor's drag and drop and thumbnails use them).
- **Effects (`Vfx`, Oct 25, 2026).** `Vfx::Play(scene, effectHandle, position)` (`Scene/Vfx.h`) instantiates a loaded effect prefab at `position`, makes its emitter a one-shot (`DestroyWhenFinished`) and returns the entity; it never loads (load the prefab once, e.g. in a script's `OnCreate`), so a prefab nobody loaded logs once and returns `{}`. Creating entities is allowed while scripts run, so it can be called from `OnUpdate` and collision callbacks.
- **Missing fields keep their defaults**, so older files still load after a component gains a field.
- Native scripts are saved by their `ScriptRegistry` name; scripts' own variables aren't saved.
- **Physics components** are saved by value (`Rigidbody2DComponent`: body type by name, flags, scales and damping; `BoxCollider2DComponent` / `CircleCollider2DComponent`: offset, size or radius, density, friction, restitution, `IsSensor`). Loading pulls values into range (sizes at least 0.001, density, friction, restitution and damping at least 0, NaN becomes the minimum). They are in `CopyableComponents`, so Play and Duplicate copy them. Runtime body handles will not be saved or copied.
- **Sprite textures and animation sets** are saved as `TextureHandle: <id>` (plus the sprite's name) and `AnimationsHandle: <id>`, and loaded through `AssetManager::GetAsset`, so sprites using one file share one texture. Scenes saved before handles have `Texture: <path>` / `Animations: <path>`; those are still read (the file is imported to get its handle) and are replaced by the handle keys the next time the scene is saved. A handle the registry doesn't know keeps its value when saved again, so a texture that is missing today can come back.

## Sprite animation

Three pieces, chained by name: a sheet texture (sprite mode `Multiple`, see [Architecture](Architecture.md#sprite-sheets-and-import-settings)), a `.hyanim` file of named clips over its sprites, and a `SpriteAnimatorComponent` that plays a clip on the entity's `SpriteRendererComponent`.

- **`SpriteAnimationSet`** (`Scene/SpriteAnimation.h`): a resource made from `SpriteAnimationSetSpecification` (texture, clips, default clip), loaded with `SpriteAnimationSet::LoadFromFile(path)` (shared while alive, `nullptr` on failure; `SpriteAnimationSerializer` reads the YAML). A clip is a name, frames (sprite index + duration in seconds, at least `MinFrameDuration`), a loop mode (`Once`, `Loop`, `PingPong`), `FlipX` and events (frame + name; recorded in the file, not delivered yet). In the file, frames name sprites (`Farmer_Left_0`), so re-slicing the sheet keeps clips; `FrameRate` is the short form for equal frames. Give every character a set with the same clip names and one script drives any of them.
- **`SpriteAnimatorComponent`**: saved `Animations` (an `AssetHandle`), `StartClip` (empty = the set's default), `PlayOnStart`, `Speed`; playback in `State` (`SpriteAnimatorState`: `ClipIndex`, `FrameIndex`, `FrameTime`, `Step`, `Playing`, `Finished`), which is not saved and is reset for every animator by `ResetSpriteAnimations(scene)` when the game starts (`Scene::OnRuntimeStart`), so a play copy and a loaded scene start fresh ([component state rule](#where-runtime-state-lives-component-state-rule) 2). Only `UpdateSpriteAnimations` and `SpriteAnimator` write `State`.
- **`UpdateSpriteAnimations(scene, context)`** (`Scene/SpriteAnimator.h`): one system over `SpriteAnimatorComponent` + `SpriteRendererComponent`, in `OnUpdateRuntime` after the scripts and physics (a `Play` from a script shows the same frame). Starts `PlayOnStart` animators, advances `FrameTime` by `context.DeltaTime * Speed` (a `while`, so a hitch can pass several frames), then writes `Texture`, `SpriteIndex` and `FlipX`. Nothing animates while editing; the inspector writes the start clip's first frame into the sprite so the editor shows the starting pose.
- **`SpriteAnimator`**: static functions over the component (no state): `Play(entity, "WalkLeft", restart = false)` (already playing that clip → carries on, so it is safe every frame; unknown name → warning once, `false`), `Play(entity, index)`, `Stop` (back to frame 0), `Pause`, `Resume`, `IsPlaying(entity, clip = {})`, `IsFinished`, `GetCurrentClip`, `GetFrameIndex`, `SetSpeed` (writes the saved `Speed`: meant for play mode, where the scene is a copy). For the editor: `ResetPlayback` (the inspector, when the animation set changes) and `RestartAfterReload(entity, before)` (after the Animation window saves a set: keeps the clip with the same name, from its first frame). The inspector's "Playing ..." line reads through these, not from `State`.

```cpp
SpriteAnimator::Play(GetEntity(), moving ? "WalkLeft" : "IdleLeft");
```

Oxygen's `VillagerWalkScript` and `Assets/Scenes/Villagers.hyscene` (Sandbox project) are a working example. `Assets/Scenes/LightingTest.hyscene` is a small playable scene built on it: the Player (`Player.png`, with idle and walk rows) uses `VillagerWalk` + `YSort`, and the camera uses `CameraFollow`; press Play and walk with WASD. In the editor: the Inspector's image import settings and the **Sprite Editor** make the sheet, the **Animation** window edits the clips (see [Architecture](Architecture.md#the-editor-oxygen)).

## System context and stages

Every engine system gets the same small struct (`Scene/SystemContext.h`) instead of a growing parameter list, and `Scene` runs its systems as numbered, plain function calls in `OnUpdateRuntime` / `OnUpdateEditor`, so the frame order above is in code, not only on this page:

```cpp
struct SystemContext
{
    float DeltaTime = 0.0f;       // seconds since the last frame: Time::GetDeltaTime(), clamped
    float FixedDeltaTime = 1/60;  // the step fixed-timestep stages (physics) advance by
    uint64_t FrameIndex = 0;      // Time::GetFrameCount()
    bool IsPlaying = false;       // true in OnUpdateRuntime, false while editing
};

void UpdateScripts(Scene& scene, const SystemContext& context);          // Scene/ScriptSystem.h
void UpdateSpriteAnimations(Scene& scene, const SystemContext& context); // Scene/SpriteAnimator.h
```

- **One context per update.** `Scene` builds it once and passes the same one to every stage, so all stages of a frame agree.
- **Stage order:** Scripts → Physics → Sprite animation → Transforms (world-transform propagation, S3) → Particles, then the application's systems, then render submission.
- **Play-only stages check `context.IsPlaying`** and return at once when it is false, so calling one while editing is harmless.
- **Starting the game:** `Scene::OnRuntimeStart` builds the `PhysicsScene` and resets every animator's `State` (`ResetSpriteAnimations`). The editor calls it when Play starts; `OnUpdateRuntime` calls it before its first stage if nobody did, so it never undoes what scripts start in their first update.
- **Physics (2D, Box2D)**: `Scene::OnRuntimeStart` builds a `PhysicsScene`, `OnRuntimeStop` / the destructor destroys it. Every entity with a Transform and a Rigidbody2D, Box or Circle collider becomes a body (a collider alone is static); entities that get those components while playing (a script's `DuplicateEntity`, a prefab, `AddComponent`) get a body at the start of the next physics step, or at once if game code calls `Physics2D::SetLinearVelocity` / `HasBody` on them. The components are read when the body is made, later changes to them don't reach the body.
  - **Who owns the transform:** *Dynamic*: the simulation (it overwrites the transform each update; move it with `Physics2D::SetLinearVelocity`). *Kinematic*: the game (write the transform; the body is carried there with the right velocity so it pushes instead of teleporting). *Static*: nobody (moving it in play mode doesn't move the collider). Inspector edits to a dynamic body's transform while playing are ignored by the body.
  - Steps are `FixedDeltaTime` (60 Hz), at most 4 per frame, the rest of a long frame is dropped. No interpolation yet: a frame with no step shows the last result. World gravity is -9.81 on Y; top-down bodies use GravityScale 0.
  - `Scene::DestroyEntity` removes the entity's body. 
  - **Collision and trigger callbacks (E2):** after each step `PhysicsScene` reads Box2D's contact and sensor events; `Scene::DispatchPhysicsEvents` then calls the scripts of both entities (`OnCollisionEnter/Exit(other)` for two solid colliders starting or stopping to touch, `OnTriggerEnter/Exit(other)` for a sensor collider and another shape starting or stopping to overlap) inside a stage, so `DestroyEntity` from a callback is deferred like in `OnUpdate`. Sensors only see shapes that can move (dynamic or kinematic bodies), never static ones, and not other sensors. Box2D 3.1 has these event flags off by default; `PhysicsScene` sets them (sensor events on sensors and on non-static shapes, contact events on solid shapes). Contact points and normals are not passed on yet.
- The list stays plain function calls. Promote it to a registered list only if applications need their own stages.
- **Particles (Oct 25, 2026, Roadmap V2).** `UpdateParticles(scene, context)` (`Scene/ParticleScene.h`) is stage 5, after Transforms (emitters read their final position). The live particles live in the scene's `ParticleScene` ([component state rule](#where-runtime-state-lives-component-state-rule) 3), made by `OnRuntimeStart` and dropped by `OnRuntimeStop`, like the `PhysicsScene`: one recycled structure-of-arrays buffer per emitter (no allocation per frame once warm), at most `ParticleScene::MaxLiveParticles` (8192) per scene, extra particles dropped and counted. Emission: `BurstCount` at the start of each loop plus `Rate` per second (fractions carried over); a non-looping emitter stops after `Duration`. Particles are world space; dead ones are swap-removed. A `DestroyWhenFinished` emitter that has emitted, stopped and has no particles left is destroyed after the loop. Game code controls emitters with `Particles2D::Play / Stop / Burst / IsPlaying / GetAliveCount(entity)`, which work before the emitter's first update too. Planned (V6): the stage also runs in `OnUpdateEditor` for previews, advancing only the emitters the editor previews. Effects started by application systems (which run after the scene update) begin emitting the next frame. Design: [Particles and VFX](https://github.com/joelnl74/Hydro/wiki/Roadmap#particles-and-vfx).

## Planned: component registry

Roadmap A6 (E4). Today a component is listed in four places: `EntitySerializer` (both directions), `CopyableComponents`, the inspector and the "Add Component" menu. One registry replaces them: per component a display name, save and load functions and an inspector draw function, registered once. Saving, loading, the play copy and the inspector loop over that list, and adding a component becomes one registration.

## Test bed scene

`Oxygen/project/Assets/Scenes/StatueVillage.hyscene` (Sandbox project, the start scene) is the engine's gameplay test bed: a small playable village where every system can be seen working. It is a game scene, not a test area. When a new or drastic system lands (physics, audio, tilemaps, UI...), it is added to the village as a gameplay element, so it is checked in the place it will be used and nothing falls out of date: physics means colliders on buildings, trees and props, a body on the player and trigger zones; audio means footsteps and ambience; a tilemap means the paths. The full list of features, controls and what to check is on the [Test bed](TestBed.md) page.

**Rules for changing it**
- **Add, don't replace.** Existing content stays, so each feature keeps being exercised. The scene is saved by the editor, or edited as YAML by hand (every entity needs a new unique UUID).
- **Make it part of the game.** A new feature gets a gameplay reason to exist in the village (an obstacle, a pushable crate, a zone, a sound source), not a separate demo area. Entities that exist only for the feature and are invisible (map walls, trigger zones) get a descriptive tag prefix (`Wall *`, `Zone *`, `Water *`) so the hierarchy groups them.
- **In the game's own style and assets.** Reuse the art in `Assets/StatueVillage`; if something is missing, add a new asset in the same style (32 px per world unit, a `.hymeta` beside it, a texture handle in the scene). The East Meadow's ground is such an asset: the village's east edge, mirrored, so the road continues.
- **Keep the map rules.** The walkable map is `VillageMap` in `Oxygen/src/Scripts/VillageScripts.h` (x -30 to 96, y -18 to 18: the village, the East Meadow and the Slime Field). The camera is clamped to it and the `Wall *` entities stop bodies at its edge; if the map grows, change both. `VillageMap::SortOriginY` stays 18, because every static sprite's `SortOrder` is `4 * (18 - feet y)`.
- **Physics content is top-down.** Bodies that move on the ground have `GravityScale 0` and a high `LinearDamping`; collider sizes and offsets are in the entity's local units (the physics step multiplies them by the scale); a collider without a Rigidbody is a static body; pushable props carry `VillageYSort` so they stay correctly sorted as they move.
- **Update the Test bed page**: add a row to its Features table (where in the scene, how to interact, what to check) and, if there are new controls, to its Controls table. Move the item out of "Planned for the test bed".
- **A feature that needs code to exercise it** (a debug toggle, a gameplay script) gets that code in `Oxygen/src/Scripts`, registered with `ScriptRegistry`, and an entity in the village bound to it.
- A feature's "done when" includes its place in the village, so the change can be checked by pressing Play.

## Adding a feature

1. **Component**: add a plain struct to `Components.h` (engine) or to your application (game-only data, like `GalaxyStar`).
2. **System**: write a function over a view. Engine systems are called from `Scene::OnUpdateEditor` or `Scene::OnUpdateRuntime`; application systems from your layer's `OnUpdate`.
3. **Rendering**: submit through `Renderer2D` from a render system; never record Vulkan commands in a system.
4. **Saving**: if the component should persist, add it to `EntitySerializer` (both directions).
5. **Play mode**: if it should be in the play copy, add it to `CopyableComponents` in `Components.h`.
6. **Editor**: show and edit the component in the inspector window.
7. **Test bed**: add the feature to the village in `StatueVillage.hyscene` as a gameplay element and a row to the [Test bed](TestBed.md) Features table.

## Known gaps

- `Scene::m_Registry` is public ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- Animation events are saved in `.hyanim` files but not delivered yet (planned: a `ScriptableEntity::OnAnimationEvent(name)` hook fed by `UpdateSpriteAnimations`).
- Scripts must be compiled into the application and registered in code: there's no separate game module or hot reload, and scripts can't expose fields to the inspector yet.
- There is no parent/child hierarchy yet.
- The editor camera reads the keyboard through `Input`, not ImGui: in play mode, flying the Editor view (right button held) also sends W/A/S/D/Q/E to the game's scripts. The camera's pose isn't saved with the session (a reopened editor starts at the default view).
- `SpriteAnimatorComponent` keeps its playback in the component ([component state rule](#where-runtime-state-lives-component-state-rule) 2). Move it to a `...Scene` object (rule 3) only if animation gains state rule 2 can't hold, such as blend layers or the AN1 state machine's parameters.
- The stage order lives in `OnUpdateRuntime` / `OnUpdateEditor` code as plain calls, not a registered list; the application's own systems (`UpdateGalaxy`) still take `(registry, deltaTime)` and are migrated when touched.
- Each component is listed separately in the serializer, `CopyableComponents` and the inspector (A6).
- Sprite textures are referenced by path until the asset system: `EntitySerializer` loads them and reads `Project` for the asset folder, a texture whose file is missing at load is dropped when the scene is saved again, and sampling options aren't saved (see [Architecture](Architecture.md#asset-system)).
