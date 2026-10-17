# Scene, ECS and systems

A scene is an [EnTT](https://github.com/skypjack/entt) registry. Entities are IDs, components are plain data, and behaviour lives in **systems**: functions that run over every entity with a given set of components, in a fixed order, each frame.

"System" in Hydro always means an ECS system like this. Engine-wide services with a lifetime (`Renderer`, `Log`, `Time`, ...) are **subsystems**; see [Subsystems](Subsystems.md). Systems get the frame's delta time as a parameter; it comes from `Time::GetDeltaTime()`.

## The pieces

| Piece | Where | What it is |
| --- | --- | --- |
| `Scene` | `Scene/Scene.h` | Owns the `entt::registry`; creates and destroys entities (`CreateEntity`, `DestroyEntity`); runs the scene's systems in `OnUpdateRuntime` / `OnUpdateEditor`; knows its viewport size (`OnViewportResize`) and primary camera (`GetPrimaryCameraEntity`) |
| `Entity` | `Scene/Entity.h` | A handle (`entt::entity` + `Scene*`). Cheap to copy, store it by value |
| Components | `Scene/Components/Components.h` | Plain structs: `TagComponent`, `IDComponent`, `TransformComponent`, `SpriteRendererComponent`, `CameraComponent`, `NativeScriptComponent` |
| Systems | `Scene::OnUpdateRuntime` / `OnUpdateEditor`, `SceneRenderer`, application layers | Functions over registry views |
| `SceneRenderer` | `Renderer/SceneRenderer.h` | Submits a scene to `Renderer2D` through a camera: `RenderEditor(editorCamera)` or `RenderRuntime()` (primary camera) |
| Scripts | `Scene/ScriptableEntity.h` | Per-entity C++ behaviour, run by the script system |
| Serialization | `Scene/SceneSerializer.h`, `Scene/PrefabSerializer.h`, `Scene/EntitySerializer.h` | Saves scenes (`.hyscene`) and prefabs (`.hyprefab`) as YAML |

Every entity created with `Scene::CreateEntity` gets a `TagComponent` (its name) and an `IDComponent` (a 64-bit `UUID` that stays the same across saves).

## Components are data

- Public fields with defaults, plus small pure helpers (`TransformComponent::GetTransform()`). No virtual functions, and no update logic.
- No raw pointers or references to other components or entities. Refer to other entities by `Entity` or `UUID`.
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
- `Scene::OnUpdateEditor(dt)` — editing: the engine systems only, no scripts. Oxygen always edits (there's no play mode yet), so it calls this one.

| Order | System | Runs in | Reads → writes |
| --- | --- | --- | --- |
| 1 | Native scripts (runtime only) | `Scene::OnUpdateRuntime` | `NativeScriptComponent` → whatever scripts touch |
| 2 | Engine systems | `Scene::OnUpdateEditor` (also called by `OnUpdateRuntime`) | None yet; transform propagation goes here |
| 3 | Application systems | The layer, after the scene update | Its own components. The editor's `UpdateGalaxy` reads `GalaxyStar` and writes `TransformComponent` + `SpriteRendererComponent` |
| 4 | Sprite submission | `SceneRenderer::RenderEditor` / `RenderRuntime` | camera → `Renderer2D::BeginScene`; `TransformComponent` + `SpriteRendererComponent` → `Renderer2D::DrawQuad` |
| — | GPU | `Renderer::RenderFrame` (application loop) | Records and presents everything submitted |

New engine systems go into `Scene::OnUpdateEditor` when they must also run while editing, or into `Scene::OnUpdateRuntime` (after the scripts) when they are game-only, in a deliberate position. Gameplay systems belong to the application and run after it.

## Cameras

A scene is drawn through exactly one camera per frame (there is one 2D pass, so the last `Renderer2D::BeginScene` of the frame wins).

| Camera | Where | Used by |
| --- | --- | --- |
| `CameraComponent` (holds a `SceneCamera`) | `Scene/Components/Components.h`, `Renderer/SceneCamera.h` | The game: `SceneRenderer::RenderRuntime` draws through the first camera with `Primary` set |
| `EditorCamera` | `Renderer/EditorCamera.h` | The editor viewport: `SceneRenderer::RenderEditor(camera)`. Not part of the scene and not saved |

- **Data only.** `SceneCamera` is the projection settings (perspective FOV/near/far, orthographic size/near/far, default orthographic with 10 units visible vertically). The camera's position and rotation come from the entity's `TransformComponent` (scale is ignored); its aspect ratio comes from the scene's viewport, set by `Scene::OnViewportResize` (`SceneRenderer::SetViewportSize` calls it). Nothing has to be kept in sync by hand.
- **One primary camera.** `Scene::GetPrimaryCameraEntity` returns the first `CameraComponent` with `Primary` set. The editor unticks the others when you tick one. Without a primary camera, `RenderRuntime` draws nothing and returns `false`.
- **Clip space is handled once**, in `Camera` (`Renderer/Camera.h`): every projection is built by `Camera::Perspective` / `Camera::Orthographic`, which use GLM's zero-to-one depth (`*_ZO`) and flip Y for Vulkan. World +Y is up on screen, geometry is counter-clockwise (`VulkanPipeline` front face), and the rendered image is shown without flipping its UVs. Never build a projection with `glm::perspective`/`glm::ortho` directly.
- **World units, not pixels.** Sprites are sized in world units (a default sprite is 1×1). The editor's galaxy fits the default 10-unit view.
- **Editor camera controls** (Oxygen viewport, **Camera** menu): Alt + left drag orbits (perspective), middle drag pans, Alt + right drag or the wheel zooms. **2D (Orthographic)** looks straight down −Z; **Scene Camera (Primary)** shows the view the game will have.

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
        transform.Translation.x += 100.0f * deltaTime;
    }
};

entity.AddComponent<NativeScriptComponent>().Bind<PlayerController>();
```

- `NativeScriptComponent` is data: a list of factories plus the instances they made. Instances are created on the scene's next update, live on the heap, and hold their `Entity` by value.
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
- Native scripts aren't saved yet ([#43](https://github.com/joelnl74/Hydro/issues/43)).

## Adding a feature

1. **Component**: add a plain struct to `Components.h` (engine) or to your application (game-only data, like `GalaxyStar`).
2. **System**: write a function over a view. Engine systems are called from `Scene::OnUpdateEditor` or `Scene::OnUpdateRuntime`; application systems from your layer's `OnUpdate`.
3. **Rendering**: submit through `Renderer2D` from a render system; never record Vulkan commands in a system.
4. **Saving**: if the component should persist, add it to `EntitySerializer` (both directions).
5. **Editor**: show and edit the component in the inspector window.

## Known gaps

- `Scene::m_Registry` is public ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- There is no play mode yet (S5), so scripts never run in the editor; a runtime application calls `OnUpdateRuntime` and `RenderRuntime`.
- Native scripts aren't serialized ([#43](https://github.com/joelnl74/Hydro/issues/43)).
- There is no parent/child hierarchy yet.
