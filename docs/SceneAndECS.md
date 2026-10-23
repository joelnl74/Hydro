# Scene, ECS and systems

A scene is an [EnTT](https://github.com/skypjack/entt) registry. Entities are IDs, components are plain data, and behaviour lives in **systems**: functions that run over every entity with a given set of components, in a fixed order, each frame.

"System" in Hydro always means an ECS system like this. Engine-wide services with a lifetime (`Renderer`, `Log`, `Time`, ...) are **subsystems**; see [Subsystems](Subsystems.md). Systems get the frame's delta time as a parameter; it comes from `Time::GetDeltaTime()`.

## The pieces

| Piece | Where | What it is |
| --- | --- | --- |
| `Scene` | `Scene/Scene.h` | Owns the `entt::registry`; creates and destroys entities (`CreateEntity`, `DestroyEntity`); runs the scene's systems in `OnUpdateRuntime` / `OnUpdateEditor`; knows its viewport size (`OnViewportResize`) and primary camera (`GetPrimaryCameraEntity`) |
| `Entity` | `Scene/Entity.h` | A handle (`entt::entity` + `Scene*`). Cheap to copy, store it by value |
| Components | `Scene/Components/Components.h` (`NativeScriptComponent`: `Scene/ScriptableEntity.h`) | Plain structs: `TagComponent`, `IDComponent`, `TransformComponent`, `SpriteRendererComponent`, `CameraComponent`, `NativeScriptComponent` |
| Systems | `Scene::OnUpdateRuntime` / `OnUpdateEditor`, `SceneRenderer`, application layers | Functions over registry views |
| `SceneRenderer` | `Renderer/SceneRenderer.h` | Submits a scene to `Renderer2D`, into a `RenderView`, through a camera: `RenderEditor(view, editorCamera)` or `RenderRuntime(view)` (primary camera) |
| Scripts | `Scene/ScriptableEntity.h`, `Scene/ScriptRegistry.h` | Per-entity C++ behaviour, run by the script system; registered by name so they can be saved and added in the editor |
| Serialization | `Scene/SceneSerializer.h`, `Scene/PrefabSerializer.h`, `Scene/EntitySerializer.h` | Saves scenes (`.hyscene`) and prefabs (`.hyprefab`) as YAML |

Every entity created with `Scene::CreateEntity` gets a `TagComponent` (its name) and an `IDComponent` (a 64-bit `UUID` that stays the same across saves).

## Components are data

- Public fields with defaults, plus small pure helpers (`TransformComponent::GetTransform()`). No virtual functions, and no update logic.
- No raw pointers or references to other components or entities. Refer to other entities by `Entity` or `UUID`.
- A shared engine resource is fine: `SpriteRendererComponent::Texture` is a `Ref<Texture2D>` (copied with the component, so the play copy shares it). It becomes an `AssetHandle` with the asset system ([Architecture](Architecture.md#asset-system)).
- Don't keep a `T&` from `GetComponent<T>()` across a call that adds or removes components. EnTT moves its storage, so the reference can dangle.

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

- `Scene::OnUpdateRuntime(dt)` — the game is running: native scripts, then the engine systems.
- `Scene::OnUpdateEditor(dt)` — editing: the engine systems only, no scripts. Oxygen calls this one except in play mode (see [Play mode](#play-mode)).

| Order | System | Runs in | Reads → writes |
| --- | --- | --- | --- |
| 1 | Native scripts (runtime only) | `Scene::OnUpdateRuntime` | `NativeScriptComponent` → whatever scripts touch |
| 2 | Engine systems | `Scene::OnUpdateEditor` (also called by `OnUpdateRuntime`) | None yet; transform propagation goes here |
| 3 | Application systems | The layer, after the scene update | Its own components. The editor's `UpdateGalaxy` reads `GalaxyStar` and writes `TransformComponent` + `SpriteRendererComponent` |
| 4 | Sprite submission | `SceneRenderer::RenderEditor` / `RenderRuntime` | camera → `Renderer2D::BeginScene`; `TransformComponent` + `SpriteRendererComponent` → `Renderer2D::DrawQuad(transform, texture, tiling, colour)` |
| — | GPU | `Renderer::RenderFrame` (application loop) | Records and presents everything submitted |

New engine systems go into `Scene::OnUpdateEditor` when they must also run while editing, or into `Scene::OnUpdateRuntime` (after the scripts) when they are game-only, in a deliberate position. Gameplay systems belong to the application and run after it.

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
- **Editor camera controls** (Oxygen's Editor window, **Camera** menu): Alt + left drag orbits (perspective), middle drag pans, Alt + right drag or the wheel zooms. **2D (Orthographic)** looks straight down −Z. The **Game** window always shows the primary camera.

## Play mode

Oxygen edits one scene and plays a copy of it, so nothing that happens while playing changes what is saved.

| Button (top bar) | What happens |
| --- | --- |
| **Play** | `Scene::Copy(editorScene)` makes the runtime scene; it becomes the active scene. Each frame: `OnUpdateRuntime` (scripts + engine systems), then the application's systems |
| **Pause** / **Resume** | Stops/continues updating the runtime scene; it is still drawn (`OnUpdateEditor` runs instead) |
| **Step** (while paused) | Runs one runtime frame |
| **Stop** | Throws the runtime scene away (its scripts get `OnDestroy`); the editor scene becomes active again |

- `Scene::Copy` copies every entity made with `CreateEntity` (same UUIDs), the components in `CopyableComponents` (`Components.h`: add new copyable components there), the native script bindings (fresh instances on the first runtime update), the name and the viewport size. Raw registry entities without an `IDComponent` are not copied; an application copies its own (the editor copies its galaxy stars).
- The hierarchy, inspector and both windows show the active scene, so the runtime scene can be inspected and tweaked while playing; those changes are lost on Stop. Saving always writes the editor scene, and opening or creating a scene stops play first.
- Application systems are gameplay: Oxygen's galaxy only moves while playing.

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
- Don't add or remove `NativeScriptComponent`s from inside a script's `OnCreate`/`OnUpdate`. The script system is iterating over them at that point ([#40](https://github.com/joelnl74/Hydro/issues/40)).
- `Scene::DestroyEntity` calls `OnDestroy` on the entity's scripts before removing it.

## Saving scenes and prefabs

Scenes and prefabs are YAML files written with yaml-cpp.

| File | Written by | Holds |
| --- | --- | --- |
| `.hyscene` | `SceneSerializer` | The scene name and every entity that has an `IDComponent` |
| `.hyprefab` | `PrefabSerializer` | One entity and its components, to spawn copies of |

- **One place per component.** `EntitySerializer` writes and reads every component for both file types. A new component that should be saved goes into `EntitySerializer::Serialize` and `EntitySerializer::Deserialize`, and nowhere else.
- **Only entities made with `CreateEntity` are saved.** Raw registry entities without an `IDComponent`, such as the editor's galaxy stars, are runtime-only.
- **UUIDs.** Loading a scene keeps the saved UUIDs. Instantiating a prefab gives each copy a new UUID, so copies never clash.
- **Missing fields keep their defaults**, so older files still load after a component gains a field.
- Native scripts are saved by their `ScriptRegistry` name; scripts' own variables aren't saved.
- **Sprite textures** are saved as `Texture: <path>`, relative to the project's asset folder when the file is inside it (otherwise absolute, forward slashes), and loaded with `Texture2D::LoadFromFile`, so sprites using one file share one texture. Textures made in code aren't saved. TODO(assets): saved as an `AssetHandle`; the loader will keep reading `Texture:` paths so old scenes still load.

## Planned: system context and stages

Roadmap A5 (finishes E5). Every system gets the same small struct instead of a growing parameter list, and `Scene` runs its systems from one ordered list, so the frame order above is in code rather than only in this page:

```cpp
struct SystemContext
{
    float DeltaTime = 0.0f;       // variable step: Time::GetDeltaTime()
    float FixedDeltaTime = 0.0f;  // fixed step for physics
    uint64_t FrameIndex = 0;
    bool IsPlaying = false;       // false in the editor
};

void UpdatePhysics(Scene& scene, const SystemContext& context);
```

Stages run in this order: **Scripts** (play mode only) → **Physics** → **Transforms** (world-transform propagation, S3) → **Render submission**. `OnUpdateRuntime` and `OnUpdateEditor` build the context and run the stages; a stage that only runs in play mode checks `context.IsPlaying`. Do this before physics (P1) adds the first engine system with its own timestep.

## Planned: component registry

Roadmap A6 (E4). Today a component is listed in four places: `EntitySerializer` (both directions), `CopyableComponents`, the inspector and the "Add Component" menu. One registry replaces them: per component a display name, save and load functions and an inspector draw function, registered once. Saving, loading, the play copy and the inspector loop over that list, and adding a component becomes one registration.

## Adding a feature

1. **Component**: add a plain struct to `Components.h` (engine) or to your application (game-only data, like `GalaxyStar`).
2. **System**: write a function over a view. Engine systems are called from `Scene::OnUpdateEditor` or `Scene::OnUpdateRuntime`; application systems from your layer's `OnUpdate`.
3. **Rendering**: submit through `Renderer2D` from a render system; never record Vulkan commands in a system.
4. **Saving**: if the component should persist, add it to `EntitySerializer` (both directions).
5. **Play mode**: if it should be in the play copy, add it to `CopyableComponents` in `Components.h`.
6. **Editor**: show and edit the component in the inspector window.

## Known gaps

- `Scene::m_Registry` is public ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- The selection is cleared on Play and Stop (there's no `GetEntityByUUID` yet to carry it over, S1).
- Scripts must be compiled into the application and registered in code: there's no separate game module or hot reload, and scripts can't expose fields to the inspector yet.
- There is no parent/child hierarchy yet.
- Systems take loose parameters (`registry`, `deltaTime`) and the order lives in `OnUpdateRuntime` / `OnUpdateEditor` code, not a list (A5).
- Each component is listed separately in the serializer, `CopyableComponents` and the inspector (A6).
- Sprite textures are referenced by path until the asset system: `EntitySerializer` loads them and reads `Project` for the asset folder, a texture whose file is missing at load is dropped when the scene is saved again, and sampling options aren't saved (see [Architecture](Architecture.md#asset-system)).
- `Entity::GetComponent` doesn't assert the component exists and has no `const` overload (E6, A11).
