# Scene, ECS and systems

A scene is an [EnTT](https://github.com/skypjack/entt) registry. Entities are IDs, components are plain data, and behaviour lives in **systems**: functions that run over every entity with a given set of components, in a fixed order, each frame.

"System" in Hydro always means an ECS system like this. Engine-wide services with a lifetime (`Renderer`, `Log`, `Time`, ...) are **subsystems**; see [Subsystems](Subsystems.md). Engine systems take `(Scene&, const SystemContext&)`; the context carries the frame's delta time (`Time::GetDeltaTime()`), the fixed step, the frame index and whether the game is playing (see [System context and stages](#system-context-and-stages)).

## The pieces

| Piece | Where | What it is |
| --- | --- | --- |
| `Scene` | `Scene/Scene.h` | Owns the `entt::registry`; creates and destroys entities (`CreateEntity`, `DestroyEntity`); runs the scene's systems in `OnUpdateRuntime` / `OnUpdateEditor`; knows its viewport size (`OnViewportResize`) and primary camera (`GetPrimaryCameraEntity`) |
| `Entity` | `Scene/Entity.h` | A handle (`entt::entity` + `Scene*`). Cheap to copy, store it by value |
| Components | `Scene/Components/Components.h` (`NativeScriptComponent`: `Scene/Scripting/ScriptableEntity.h`) | Plain structs: `TagComponent`, `IDComponent`, `RelationshipComponent` (the parent, see [Hierarchy](#hierarchy)), `TransformComponent` (local), `WorldTransformComponent` (derived), `SpriteRendererComponent`, `SpriteAnimatorComponent`, `CameraComponent`, `Light2DComponent` (Point, Global, Spot), `DirectionalLightComponent` (the sun of forward lighting), `MeshComponent`, `Shadow2DComponent`, `Rigidbody2DComponent`, `BoxCollider2DComponent`, `CircleCollider2DComponent`, the 3D colliders (`BoxColliderComponent`, `SphereColliderComponent`, `CapsuleColliderComponent`, `CylinderColliderComponent`, `MeshColliderComponent`), `CharacterBodyComponent`, `ParticleEmitter2DComponent` (settings only; its particles are in the `ParticleScene`), `NativeScriptComponent` |
| Systems | `Scene::OnUpdateRuntime` / `OnUpdateEditor`, `SceneRenderer`, application layers | Functions over registry views |
| `SceneRenderer` | `Renderer/SceneRenderer.h` | Submits a scene to `Renderer2D`, into a `RenderView`, through a camera: `RenderEditor(view, editorCamera, overlay)` or `RenderRuntime(view)` (primary camera) |
| Scripts | `Scene/Scripting/` (`ScriptableEntity.h`, `ScriptRegistry.h`, `ScriptSystem.h`, reflection types) | Per-entity C++ behaviour, run by the script system; registered by name (generated from `HY_SCRIPT`) so they can be saved and added in the editor; `HY_PROPERTY` fields are edited in the inspector and saved |
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
3. **Runtime state that is variable-size, owned by a library or the GPU, or must exist apart from the entity** lives in a per-scene `...Scene` object owned by `Scene`, and then **all** of that component's runtime state goes there (one source of truth). The component holds settings only. *Examples: `PhysicsScene` (Box2D bodies); `PhysicsScene3D` (Jolt bodies and characters); `ParticleScene` (particle buffers, planned, Roadmap V2).*
4. **Scripts and editor tools go through the facade** (`SpriteAnimator`, `Physics2D`, `Physics3D` and `CharacterBody`, `Particles2D`), never through runtime fields. Editor windows may read settings directly; they read playback or simulation state through the facade.

`Scene::Copy` copies a rule-2 `State` along with the component (it is plain data); the reset in `OnRuntimeStart` makes the game start from the settings anyway.

**Derived data** (agreed Oct 29, 2026, [Hierarchy and UI plan](https://github.com/joelnl74/Hydro/wiki/Hierarchy-and-UI-Plan) D6) is not runtime state but a result computed from authored components. It gets its own component, separate from the authored one, that is added and removed together with its source (registry construct / destroy signals, connected in the `Scene` constructor), so the pass that computes it only writes values and never changes the registry's structure. It is never saved, copied on Play or shown in the inspector. *Example: `WorldTransformComponent`, derived from `TransformComponent` and the hierarchy by `Scene::ResolveTransforms`; later `UIResolvedRectComponent` (UI1).*

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
- `Scene::OnUpdateEditor(dt)` — editing: runs only the transform stage (`ResolveTransforms`), no scripts or animation. Oxygen calls this one except in play mode (see [Play mode](#play-mode)).

| Order | System | Runs in | Reads → writes |
| --- | --- | --- | --- |
| 1 | `UpdateScripts` (runtime only) | `Scene::OnUpdateRuntime` | `NativeScriptComponent` → whatever scripts touch |
| 2 | Physics (runtime only) | `Scene::OnUpdateRuntime` | `PhysicsScene::Step` (2D): kinematic bodies are carried to where their entities are in the world (a root's transform, a child's `Scene::GetWorldTransform`), fixed 60 Hz steps run, dynamic bodies' results are written back (a root's transform; a child's through `Scene::SetWorldTransform`). Then `PhysicsScene3D::Step` (3D): new bodies, kinematic bodies moved, fixed 60 Hz steps move the characters, their positions are written blended between the last two steps (`Scene::SetWorldPosition`). Then both worlds' events go to scripts (see below) |
| 3 | `UpdateSpriteAnimations` (runtime only) | `Scene::OnUpdateRuntime`, after the scripts | `SpriteAnimatorComponent` → `SpriteRendererComponent` (`Texture`, `SpriteIndex`, `FlipX`) |
| 4 | `Scene::ResolveTransforms` | `OnUpdateEditor` and `OnUpdateRuntime`; also `OnRuntimeStart` (before the physics worlds are built) and the start of `SceneRenderer::RenderEditor` / `RenderRuntime` (so what application systems moved is drawn this frame) | `TransformComponent` + `RelationshipComponent` (through the entity list) → `WorldTransformComponent`, parents first, one pass, no dirty flags |
| 5 | `UpdateParticles` (runtime only) | `Scene::OnUpdateRuntime`, after transforms | `WorldTransformComponent` + `ParticleEmitter2DComponent` → the scene's `ParticleScene` (emits, simulates, retires particles); destroys finished `DestroyWhenFinished` entities after its loop |
| 6 | Application systems | The layer, after the scene update | Its own components. The editor's `UpdateGalaxy` reads `GalaxyStar` and writes `TransformComponent` + `SpriteRendererComponent` |
| 7 | Sprite, particle and light submission | `SceneRenderer::RenderEditor` / `RenderRuntime` | first `Scene::ResolveTransforms` (what application systems moved is drawn this frame); camera → `Renderer2D::BeginScene`; `WorldTransformComponent` + `SpriteRendererComponent` → `Renderer2D::DrawQuad(transform, texture, tiling or sprite rect, colour)`, the quad turned for its `Facing` (Upright, Billboard) when the camera is tilted; `WorldTransformComponent` + `Light2DComponent` → `Renderer2D::DrawLight` / `DrawGlobalLight`; `WorldTransformComponent` + `Shadow2DComponent` → `Renderer2D::DrawShadow` (the view is then drawn lit, see [Architecture](Architecture.md#2d-lighting)). The scene's post-processing (`Scene::GetActivePostProcess`: its own or its render profile's exposure and bloom, for HDR views) goes to `Renderer2D::SetPostProcess`. In forward-lit views lights, the main `DirectionalLightComponent` and the scene's environment (`Scene::GetActiveEnvironment`) become one `SceneLighting` (`Renderer2D::SetSceneLighting`, [Architecture](Architecture.md#forward-lighting)) and shadows are decals; with sun shadows, cut-out standing sprites also queue their shadow casters (`Renderer2D::BeginShadowCasters`, [Architecture](Architecture.md#sun-shadows)) and meshes pass `MeshComponent::CastShadows` |
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
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

HY_SCRIPT()
class PlayerController : public Hydro::ScriptableEntity
{
public:
    HY_PROPERTY()
    float MoveSpeed = 2.0f; // shown as "Move Speed" in the inspector, saved with the scene

protected:
    void OnCreate() override {}
    void OnUpdate(float deltaTime) override
    {
        auto& transform = GetComponent<TransformComponent>();
        transform.Translation.x += MoveSpeed * deltaTime; // world units per second
    }
};

// Bind by type in code, or by name (what loading a scene and the editor do):
entity.AddComponent<NativeScriptComponent>().Bind<PlayerController>().Bind("HealthScript");
```

### Script reflection (`HY_SCRIPT` / `HY_PROPERTY`, Oct 28, 2026)

Scripts are no longer registered by hand. A code generator finds annotated classes and writes their registration:

- **`HY_SCRIPT()`** above a class that derives publicly from `ScriptableEntity` registers it under its **class name** (`SpinScript`, `DioramaWalkScript`). That name is what scenes and prefabs save, so renaming the class breaks saved scenes (there are no aliases yet).
- **`HY_PROPERTY()`** above a **public** field of such a class exposes it: the inspector shows it under the script's name, the value is saved per binding in the scene, and it is pushed into the instance before `OnCreate`. Types (V2, Oct 29, 2026): `bool`, `int`, `float`, `glm::vec2`, `glm::vec3`, `glm::vec4`, `std::string` and `AssetHandle`. One field per declaration. The saved key is the field name; the label is the field name without `m_`, split at capitals (`m_MoveSpeed` -> "Move Speed").
- **Arguments** (V2, any order, all optional): `Range(min, max)`, `Min(v)`, `Max(v)` (int, float and each vector component, clamped when the field is left), `Tooltip("...")`, `DisplayName("...")`, `Color` (a `vec3` / `vec4` edited with a colour picker), `Asset(Texture2D)` (required for an `AssetHandle`: the `AssetType` its picker lists), `ReadOnly`, `Hidden` (saved and applied, not shown). Example: `HY_PROPERTY(Range(0, 20), Tooltip("Metres per second")) float Speed = 3.5f;`. The full list is in `ReflectionMacros.h`.
- **During Play** the inspector shows each field's live value (read back from the running instance with `ScriptProperty::Read`) and an edit is applied to the instance at once (`Apply`). The edits are made on the play copy, so Stop discards them like every other Play edit.
- **Asset handles** are only stored: the script loads what it needs (`AssetManager::GetAsset<Texture2D>(Icon)`), typically in `OnCreate`.
- The macros (`Scene/Scripting/ReflectionMacros.h`) expand to nothing; they are only markers for `Tools/ReflectionGenerator/generate_reflection.py`. Oxygen runs it as a **pre-build step** (`Oxygen/premake5.lua`), it scans `Hydro/src` and `Oxygen/src` headers and writes `Oxygen/src/Generated/GeneratedScriptReflection.cpp` (`RegisterGeneratedScriptReflection()`, safe to call twice) only when the output changes. Another application passes its own folders and file: `--source <dir>` (repeatable) and `--output <file>`; `--check` only reports whether the file is up to date (exit 1 if not, for CI). Comments are ignored, `final` classes are fine, and the build fails with the file and line on anything it doesn't understand: a non-public or static field, an unsupported type, a comma declaration (`float A, B;`), `HY_PROPERTY` inside a nested type, an unknown argument, a `HY_SCRIPT()` not directly above its class.
- **Default values** come from a default-constructed instance (`ScriptRegistration<T>` keeps one), so the initialiser in the class (`= 2.0f`) is the default. A scene that saved no value for a property, or was saved before the property existed, gets the default.
- **Manual registration** still works and goes through the same path: `ScriptRegistry::Register<T>("Name").Property("Speed", &T::Speed)` (or with a separate display name), followed by modifiers that change the property just added: `.Range(min, max)`, `.Min`, `.Max`, `.Tooltip("...")`, `.Color()`, `.AssetFilter(AssetType::Texture2D)`, `.ReadOnly()`, `.Hidden()`. Use it for scripts the generator can't see.
- **Types**: `ScriptTypeInfo` (name, `std::type_index`, factory, `std::vector<ScriptProperty>`), `ScriptProperty` (name, display name, `ScriptPropertyType`, default, the inspector options, `Apply` that writes the value into an instance and `Read` that reads it back), `ScriptPropertyValue` (`std::variant<bool, int, float, glm::vec2, glm::vec3, glm::vec4, std::string, AssetHandle>`, in the order of `ScriptPropertyType`), `ScriptPropertyMap` (name -> value, one per binding in `NativeScriptComponent::Script::Properties`).

- `NativeScriptComponent` is data: a list of scripts (name, factory, instance, and the binding's property values). An entity has one `NativeScriptComponent` but any number of scripts in it. Instances are created on the scene's next runtime update, live on the heap, and hold their `Entity` by value.
- **`ScriptRegistry`** maps a name to a script's `ScriptTypeInfo` (factory and properties). Scenes and prefabs save each binding as `{ Name: SpinScript, Properties: { Speed: 3.5 } }` (the old list of plain names still loads) and loading binds them again by name. Every script must be registered before a scene that uses it is loaded (Oxygen calls `RegisterGeneratedScriptReflection()` in `OxygenEditorApplication::OnInit`, before it pushes the editor layer); names must stay stable once saved. A script bound in code but not registered still runs, but isn't saved. A saved name that isn't registered is kept (so the file saves back unchanged) but doesn't run, and the editor shows it in red.
- Remove scripts with `Scene::RemoveScript(entity, index)` / `Scene::RemoveScripts(entity)`, which call `OnDestroy` on running ones. Oxygen's inspector lists an entity's scripts, removes them, and adds registered ones (**Add Component > Script** or **Add Script**).
- **Scripts can change the scene.** From `OnCreate`/`OnUpdate` a script may create entities, add components, bind scripts, and destroy entities or remove scripts, including its own. The script stage (`UpdateScripts`) walks a snapshot of the entities that had scripts when it began, and two rules keep it safe ([#40](https://github.com/joelnl74/Hydro/issues/40)):
  - **Created things start next update.** An entity made during the stage exists at once (you can add components to it), but its scripts first run on the next update. A script bound to an existing entity does the same.
  - **Removal happens when the stage ends.** While the stage runs, `Scene::DestroyEntity`, `RemoveScript` and `RemoveScripts` only mark the entity or script: it stops running at once (no more `OnUpdate`), `Scene::IsPendingDestroy(entity)` is true, and the entity stays valid (`GetComponent` still works) until the stage ends. Then `OnDestroy` is called and it is removed. Calling them twice is harmless. Outside a stage (the editor's hierarchy and inspector) they act at once.
  - `OnDestroy` may itself destroy other entities or remove scripts; those are applied in the same round, after it. When a `Scene` is destroyed, such requests are dropped.
  - A script removed with `RemoveScripts` leaves an empty `NativeScriptComponent` behind once the stage ends (removing it outright would need a component removal while a stage runs).
  - `m_StageDepth` (private) is what the deferral checks; any stage that runs user code (physics callbacks, later) brackets its work with `Scene::BeginStage` / `EndStage`.
- `Scene::DestroyEntity` calls `OnDestroy` on the entity's scripts before removing it.

## Hierarchy

Roadmap S3, [Hierarchy and UI plan](https://github.com/joelnl74/Hydro/wiki/Hierarchy-and-UI-Plan) steps H1a (the tree), H1b (world transforms) and H2 (everything that places things in the world reads them), Oct 29, 2026. The editor's tree view and prefab subtrees come with H3.

- **Data.** `RelationshipComponent { UUID Parent }`; an entity without it is a root (the component is removed rather than set to 0). Saved as `RelationshipComponent: { Parent: <uuid> }`, copied on Play.
- **One order.** The scene's entity list (`GetEntityOrder`) is the tree in **depth-first pre-order**: a parent comes before its children, and an entity's whole subtree follows it directly, in one piece. The hierarchy window, saving and `Scene::Copy` already use that list, so they keep the tree for free. Children of an entity are derived from the list and the parents; nothing stores them.
- **Who does what.** `SceneHierarchy` (`Scene/SceneHierarchy.h`) is the tree: the layout of one list (parent index, depth, subtree end per entry; children, ancestors, where a moved subtree goes), the rule check and the repair, all over plain (UUID, parent UUID) pairs and list indices, never the registry. `Scene` owns one, rebuilt on first use after the list or a parent changed, and maps entities to list indices.
- **Change parents only through the scene**, which keeps the rule:
  - `SetParent(child, parent, keepWorldTransform = true)` makes `child` the last child of `parent` (an empty `Entity` makes it a root, last in the list); its subtree moves with it. By default the child's local transform is rewritten so it stays where it is in the world (Unity's default); with `false` the local values are kept. Refused (logged, `false`) for a parent inside the child's subtree.
  - `MoveEntity(entity, target, placeAfter)` moves the entity's subtree before `target` or after target's subtree, as target's sibling (it takes target's parent, keeping its place in the world).
  - `DestroyEntity` destroys the whole subtree, children first. During a stage the whole subtree stops running at once and goes when the stage ends.
  - `DuplicateEntity` copies the whole subtree (new UUIDs) right after the original's subtree, with the same parent. References inside the copied set follow the copies (`RemapEntityReferences` in `Scene.cpp`, one line per field that refers to an entity: today `RelationshipComponent::Parent`; the component registry, A6, takes it over); references to entities outside it are kept.
  - Writing `RelationshipComponent::Parent` directly breaks the rule; don't.
- **Queries:** `GetParent`, `GetChildren`, `HasChildren`, `GetSubtree` (the entity first), `IsAncestorOf`, `GetDepth`.
- **World transforms.** `TransformComponent` is **local** (relative to the parent); for a root, local and world are the same, so existing scenes and scripts are unchanged. `WorldTransformComponent { Matrix, Translation, Rotation (quat), Scale }` is the whole chain, derived data (see [the rule above](#where-runtime-state-lives-component-state-rule)): it exists exactly when a `TransformComponent` does and only `Scene::ResolveTransforms` writes it (stage 4, the start of the game, the start of rendering). An entity without a `TransformComponent` passes its parent's world on to its children. Entities outside the list (raw registry entities) are roots.
  - `Scene::GetWorldTransform(entity)` computes it now from the chain (always current, unlike the component).
  - `Scene::SetWorldTransform(entity, world)` writes the local transform that gives `world`. Under a rotated parent with non-uniform scale `world` can need shear, which a `TransformComponent` can't hold: it writes the closest translation / rotation / scale, warns once per entity and returns `false`. A parent with a zero scale is refused. Rotation keeps the entity's current Euler angles when they describe the same rotation.
  - `TransformMath` (`Scene/TransformMath.h`): `Compose`, `Decompose` (a mirror is a negative X scale), `IsTRS`, `ToEulerAngles`.
  - `Scene::GetWorldPosition(entity)` / `SetWorldPosition(entity, position)`: the position alone. Setting it changes only the local `Translation` (a root's is written as it is), so rotation and scale stay exact and no shear can appear.
- **Who reads what (H2).** Everything that places things in the world reads world transforms:
  - `SceneRenderer`: sprites, meshes, sort depth, cameras (a camera's view comes from its world position and rotation), 2D and forward lights, the sun, blob shadows, particles, selection and collider outlines. It calls `ResolveTransforms` first.
  - `UpdateParticles` (stage 5): emitter origins, from `WorldTransformComponent` (stage 4 has run).
  - Physics (stage 2, before stage 4) reads where an entity is **now**: a root's `TransformComponent` exactly as before, a child's `Scene::GetWorldTransform` (a child spawned this frame would otherwise start at the origin). Collider shapes are sized by the world scale (`ColliderShapes::Get` takes it). Results go back to a root's `TransformComponent` as before; a dynamic 2D child gets `SetWorldTransform`, a character `SetWorldPosition`. Physics bodies should be roots or under parents with an even scale: the inspector warns otherwise.
  - The editor: the transform gizmo sits on the world transform and writes the local one back (`inverse(parentWorld) * result`); focus on selection frames the world position; the inspector's Transform section is local and says which parent it is relative to.
  - **Scripts** read and write `TransformComponent`, which is local. For an entity with a parent, use `Scene::GetWorldPosition` / `SetWorldPosition` (or `GetWorldTransform` / `SetWorldTransform`) when the position in the world matters. Every entity in today's scenes is a root, where the two are the same.
- **Loading repairs, never rejects.** `SceneSerializer` creates every entity first, then calls `Scene::RepairHierarchy`: a missing parent, an entity that is its own parent and one entity of every cycle (the one listed first) become roots, each with a warning, and the list is rebuilt depth-first, keeping roots and each entity's children in file order. A file Hydro wrote comes out unchanged.
- **Validation.** `Scene::ValidateHierarchy()` checks the rule and logs every entity that breaks it. Debug builds run it after every edit (create, destroy, move, reparent, duplicate, repair), so a mistake shows where it happens.
- **Prefabs** (H3) hold the entity and its whole subtree. `PrefabSerializer::Save` writes `Entities:` depth-first, the root without its parent. `Instantiate(scene, prefab, parent = {})` creates them with new UUIDs, links each to its copied parent (`SetParent`, local values kept), runs `Scene::RemapEntityReferences` over them, and puts the root last among the roots or as the last child of `parent` (its saved transform is then relative to that parent). `Prefab::LoadUncached` is the only reader of the format; files with one `Entity:` (before hierarchies) load as a prefab without children.
- **Editor** (H3, `HierarchyWindow`): the tree as described in [Architecture](Architecture.md#the-editor-oxygen). Edits from the window (drops, menus) wait until the list has been drawn, then go through `SetParent` / `MoveEntity` / `DuplicateEntity` / `DestroyEntity`.

## Saving scenes and prefabs

Scenes and prefabs are YAML files written with yaml-cpp.

| File | Written by | Holds |
| --- | --- | --- |
| `.hyscene` | `SceneSerializer` | The scene name, its `RenderSettings` (only when it overrides the project's render features), its `Look` (render profile and the source of each section, only when it uses one), its own `Environment` and `PostProcess` (only when set) and every entity that has an `IDComponent` |
| `.hyprefab` | `PrefabSerializer` | An entity and its whole subtree (`Entities:`, the root first, depth-first), to spawn copies of. Older files with one `Entity:` still load |

- **One place per component.** `EntitySerializer` writes and reads every component for both file types. A new component that should be saved goes into `EntitySerializer::Serialize` and `EntitySerializer::Deserialize`, and nowhere else.
- **Only entities made with `CreateEntity` are saved.** Raw registry entities without an `IDComponent`, such as the editor's galaxy stars, are runtime-only.
- **Render settings.** A scene can override the project's render features (`SceneRenderSettings`: `OverrideProject` + `RenderFeatureSettings`, edited in **Render Settings > Rendering (scene)**). Scenes that don't override write nothing, so older files and 2D scenes stay as they were. See [Render features](Architecture.md#render-features-and-the-render-graph).
- **UUIDs.** Loading a scene keeps the saved UUIDs. Instantiating a prefab gives each copy a new UUID, so copies never clash.
- **Prefabs as loaded assets (Oct 25, 2026).** `AssetManager::GetAsset<Prefab>(handle)` reads a `.hyprefab` once into a `Prefab` (`Scene/Prefab.h`: its name and the saved entities as YAML) and keeps it like a texture; it loads again when the file changes. `PrefabSerializer::Instantiate(scene, prefab)` makes a copy from a loaded `Prefab` without reading the file or logging; `Instantiate(scene, handle)` loads it first when needed. The path-based `Instantiate` / `Load` still read the file every time (the editor's drag and drop and thumbnails use them).
- **Effects (`Vfx`, Oct 25, 2026).** `Vfx::Play(scene, effectHandle, position)` (`Scene/Vfx.h`) instantiates a loaded effect prefab at `position`, makes its emitter a one-shot (`DestroyWhenFinished`) and returns the entity; it never loads (load the prefab once, e.g. in a script's `OnCreate`), so a prefab nobody loaded logs once and returns `{}`. Creating entities is allowed while scripts run, so it can be called from `OnUpdate` and collision callbacks.
- **Missing fields keep their defaults**, so older files still load after a component gains a field.
- Native scripts are saved by their `ScriptRegistry` name with their `HY_PROPERTY` values (`Properties`, only when the script has any). Loading reads only the properties the registered script still has; other fields of the script aren't saved.
- **Physics components** are saved by value (`Rigidbody2DComponent`: body type by name, flags, scales and damping; `BoxCollider2DComponent` / `CircleCollider2DComponent`: offset, size or radius, density, friction, restitution, `IsSensor`; the 3D colliders: offset, size / radius / height, `Layer` by name, `IsTrigger`; `MeshColliderComponent`: `Layer`; `CharacterBodyComponent`: radius, height, `MaxSlopeDegrees`, `StepHeight`, `StaysOnLedges`, `Layer`). Loading pulls values into range (sizes at least 0.001, density, friction, restitution and damping at least 0, NaN becomes the minimum). They are in `CopyableComponents`, so Play and Duplicate copy them. Runtime body handles will not be saved or copied.
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

void UpdateScripts(Scene& scene, const SystemContext& context);          // Scene/Scripting/ScriptSystem.h
void UpdateSpriteAnimations(Scene& scene, const SystemContext& context); // Scene/SpriteAnimator.h
```

- **One context per update.** `Scene` builds it once and passes the same one to every stage, so all stages of a frame agree.
- **Stage order:** Scripts → Physics → Sprite animation → Transforms (`ResolveTransforms`: world transforms from the hierarchy) → Particles, then the application's systems, then render submission.
- **Play-only stages check `context.IsPlaying`** and return at once when it is false, so calling one while editing is harmless.
- **Starting the game:** `Scene::OnRuntimeStart` builds the `PhysicsScene` and the `PhysicsScene3D` and resets every animator's `State` (`ResetSpriteAnimations`). The editor calls it when Play starts; `OnUpdateRuntime` calls it before its first stage if nobody did, so it never undoes what scripts start in their first update.
- **Physics (2D, Box2D)**: `Scene::OnRuntimeStart` builds a `PhysicsScene`, `OnRuntimeStop` / the destructor destroys it. Every entity with a Transform and a Rigidbody2D, Box or Circle collider becomes a body (a collider alone is static); entities that get those components while playing (a script's `DuplicateEntity`, a prefab, `AddComponent`) get a body at the start of the next physics step, or at once if game code calls `Physics2D::SetLinearVelocity` / `HasBody` on them. The components are read when the body is made, later changes to them don't reach the body.
  - **Who owns the transform:** *Dynamic*: the simulation (it overwrites the transform each update; move it with `Physics2D::SetLinearVelocity`). *Kinematic*: the game (write the transform; the body is carried there with the right velocity so it pushes instead of teleporting). *Static*: nobody (moving it in play mode doesn't move the collider). Inspector edits to a dynamic body's transform while playing are ignored by the body.
  - Steps are `FixedDeltaTime` (60 Hz), at most 4 per frame, the rest of a long frame is dropped. No interpolation yet: a frame with no step shows the last result. World gravity is -9.81 on Y; top-down bodies use GravityScale 0.
  - `Scene::DestroyEntity` removes the entity's body. 
  - **Physics (3D, Jolt; World collision plan, Jolt physics MVP):** `Scene::OnRuntimeStart` also builds a `PhysicsScene3D` (`Physics/PhysicsScene3D.*`), destroyed like the 2D one. Z up, metres, gravity -Z, fixed 60 Hz steps (at most 4 a frame). Collision is opt-in:
    - **Colliders:** every entity with a Transform and 3D collider components is a body: `BoxColliderComponent`, `SphereColliderComponent`, `CapsuleColliderComponent`, `CylinderColliderComponent` (offset = the shape's centre and sizes in the entity's local units, scaled and turned by its transform when the body is made), and `MeshColliderComponent` (the `MeshComponent` as it is: built-in shapes as themselves, `Stairs` as a ramp through the middle of the treads, `Plane` as a thin slab; a model through its collision nodes (WC3: glTF nodes named `..._COL` or `COL_...` are solid convex hulls, `..._WALK` or `COL_..._WALK...` walkable triangles; the importer keeps them apart and never draws them), or as its bounding box when it has none). `Scene/ColliderShapes` turns the components into shapes, for the physics world and the editor's outlines alike. A body is static, or kinematic when the entity has a `NativeScriptComponent` (it follows its transform). `IsTrigger` colliders form a second body on the `Trigger` layer that blocks nothing. `PhysicsLayer` (World, Player, Npc, Trigger): the world doesn't meet itself, triggers meet nothing, the rest meets.
    - **Characters:** an entity with a `CharacterBodyComponent` is a Jolt `CharacterVirtual`: a capsule standing on the entity's position (its feet; the transform's scale doesn't size it) that slides along walls, walks slopes up to `MaxSlopeDegrees`, steps up and down `StepHeight`, sticks to the ground, and with `StaysOnLedges` never walks off a drop (it stops or slides along the edge). Scripts say where it wants to go with `CharacterBody::Move(entity, velocity)` in stage 1; the physics stage moves it in every fixed step and writes the transform **blended between the last two steps** (interpolation: smooth at any frame rate, one step behind). Writing the transform teleports it. `CharacterBody::GetGround` says what it stands on.
    - **Queries:** `Physics3D::Raycast(scene, from, to, layerMask, hit)` (entity, point, normal, distance) and `Physics3D::OverlapSphere(scene, centre, radius, layerMask, entities)`; masks are `Physics3D::LayerBit(layer)`, `SolidLayers`, `AllLayers`. Ray casts see triggers only when the mask asks for them.
    - **Trigger events:** after each step, a character that walked into or out of a trigger sends `OnTriggerEnter/Exit(other)` to the scripts of both, through `Scene::DispatchPhysicsEvents` with the 2D events. Collision callbacks for characters are not sent.
  - **Collision and trigger callbacks (E2):** after each step `PhysicsScene` reads Box2D's contact and sensor events; `Scene::DispatchPhysicsEvents` then calls the scripts of both entities (`OnCollisionEnter/Exit(other)` for two solid colliders starting or stopping to touch, `OnTriggerEnter/Exit(other)` for a sensor collider and another shape starting or stopping to overlap) inside a stage, so `DestroyEntity` from a callback is deferred like in `OnUpdate`. Sensors only see shapes that can move (dynamic or kinematic bodies), never static ones, and not other sensors. Box2D 3.1 has these event flags off by default; `PhysicsScene` sets them (sensor events on sensors and on non-static shapes, contact events on solid shapes). Contact points and normals are not passed on yet.
- The list stays plain function calls. Promote it to a registered list only if applications need their own stages.
- **Particles (Oct 25, 2026, Roadmap V2).** `UpdateParticles(scene, context)` (`Scene/ParticleScene.h`) is stage 5, after Transforms (emitters read their final position). The live particles live in the scene's `ParticleScene` ([component state rule](#where-runtime-state-lives-component-state-rule) 3), made by `OnRuntimeStart` and dropped by `OnRuntimeStop`, like the `PhysicsScene`: one recycled structure-of-arrays buffer per emitter (no allocation per frame once warm), at most `ParticleScene::MaxLiveParticles` (8192) per scene, extra particles dropped and counted. Emission: `BurstCount` at the start of each loop plus `Rate` per second (fractions carried over); a non-looping emitter stops after `Duration`. Particles are world space; dead ones are swap-removed. A `DestroyWhenFinished` emitter that has emitted, stopped and has no particles left is destroyed after the loop. Game code controls emitters with `Particles2D::Play / Stop / Burst / IsPlaying / GetAliveCount(entity)`, which work before the emitter's first update too. Planned (V6): the stage also runs in `OnUpdateEditor` for previews, advancing only the emitters the editor previews. Effects started by application systems (which run after the scene update) begin emitting the next frame. Design: [Particles and VFX](https://github.com/joelnl74/Hydro/wiki/Roadmap#particles-and-vfx).

## Planned: component registry

Roadmap A6 (E4). Today a component is listed in four places: `EntitySerializer` (both directions), `CopyableComponents`, the inspector and the "Add Component" menu. One registry replaces them: per component a display name, save and load functions and an inspector draw function, registered once. Saving, loading, the play copy and the inspector loop over that list, and adding a component becomes one registration.

## Test bed scene

> **Oct 29, 2026:** `StatueVillage.hyscene` and its builder scripts were deleted. Until the rules below are rewritten for it, the gameplay test bed is the field scenes (`ShrineGlade.hyscene`, `FarmVillage.hyscene`, `Battle.hyscene`); read "the village" below as those scenes.

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
7. **Test bed**: add the feature to the field scenes (`ShrineGlade` / `FarmVillage`) as a gameplay element and a row to the [Test bed](TestBed.md) Features table.

## Known gaps

- `Scene::m_Registry` is public ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- Animation events are saved in `.hyanim` files but not delivered yet (planned: a `ScriptableEntity::OnAnimationEvent(name)` hook fed by `UpdateSpriteAnimations`).
- Scripts must be compiled into the application: there's no separate game module or hot reload.
- Script reflection (open points):
  - No `enum class`, `Entity` / `UUID` references, arrays or nested structs as properties yet; an `AssetHandle` isn't checked against the asset's type when the scene loads.
  - No `OnValidate` / property-changed hook: a script that derives state from a property in `OnCreate` doesn't see a live edit until it reads the field again.
  - Saved values of a script that isn't registered (renamed or deleted class) are dropped on load, so saving loses them; there are no rename aliases for script or property names ([#143](https://github.com/joelnl74/Hydro/issues/143)).
  - The generator is a text scanner, not a C++ parser: macros, `#if` blocks and templates around a script are not understood.
  - Exposed fields keep their member names as saved keys (`DioramaWalkScript::m_Speed`, now labelled "Speed"); public fields should be `PascalCase`, but renaming one needs #143's aliases first.
- Hierarchy (S3) is in (H1a-H3) but nothing in the test bed uses a parent yet (H4: street lamps, shrine lanterns). A dynamic 2D child under a rotated parent with non-uniform scale gets the closest transform without shear (the inspector warns). 3D rigid bodies don't exist yet; when they come they write back through `SetWorldTransform` like 2D. Prefabs are copies (no link back to the prefab asset, no overrides: G4). No undo for hierarchy edits yet (#165). [Hierarchy and UI plan](https://github.com/joelnl74/Hydro/wiki/Hierarchy-and-UI-Plan).
- The editor camera reads the keyboard through `Input`, not ImGui: in play mode, flying the Editor view (right button held) also sends W/A/S/D/Q/E to the game's scripts. The camera's pose isn't saved with the session (a reopened editor starts at the default view).
- `SpriteAnimatorComponent` keeps its playback in the component ([component state rule](#where-runtime-state-lives-component-state-rule) 2). Move it to a `...Scene` object (rule 3) only if animation gains state rule 2 can't hold, such as blend layers or the AN1 state machine's parameters.
- The stage order lives in `OnUpdateRuntime` / `OnUpdateEditor` code as plain calls, not a registered list; the application's own systems (`UpdateGalaxy`) still take `(registry, deltaTime)` and are migrated when touched.
- Each component is listed separately in the serializer, `CopyableComponents` and the inspector (A6).
- 3D physics (Jolt physics MVP): no rigid bodies, joints or character-versus-character collision; a model without collision nodes collides as its bounding box; collision nodes ignore a mirroring scale; the layer matrix is fixed in code (WC6); inspector edits to colliders during play don't reach the bodies; only characters send trigger events (a character destroyed inside a trigger sends the trigger `OnTriggerExit` from `Scene::DestroyEntityNow`, Oct 29; a destroyed trigger sends nothing); 3D bodies never send `OnCollisionEnter` / `OnCollisionExit` (no contact listener, [#152](https://github.com/joelnl74/Hydro/issues/152)); kinematic bodies are teleported (zero velocity), so characters don't ride or get pushed by them ([#153](https://github.com/joelnl74/Hydro/issues/153)); a collider whose shape fails to build has no collision for the rest of the run (warned once; press Play again after fixing it); removing a collider or Character Body during Play leaves its body in the world ([#151](https://github.com/joelnl74/Hydro/issues/151)). Remaining WC4-WC7 items: [#154](https://github.com/joelnl74/Hydro/issues/154). Scenes with 3D colliders keep their 2D footprints for Box2D bodies (the wisps).
- Sprite textures are referenced by path until the asset system: `EntitySerializer` loads them and reads `Project` for the asset folder, a texture whose file is missing at load is dropped when the scene is saved again, and sampling options aren't saved (see [Architecture](Architecture.md#asset-system)).
