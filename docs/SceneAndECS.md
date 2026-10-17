# Scene, ECS and systems

A scene is an [EnTT](https://github.com/skypjack/entt) registry. Entities are IDs, components are plain data, and behaviour lives in **systems**: functions that run over every entity with a given set of components, in a fixed order, each frame.

## The pieces

| Piece | Where | What it is |
| --- | --- | --- |
| `Scene` | `Scene/Scene.h` | Owns the `entt::registry`; creates entities; runs the scene's systems in `OnUpdate` |
| `Entity` | `Scene/Entity.h` | A handle (`entt::entity` + `Scene*`). Cheap to copy, store it by value |
| Components | `Scene/Components/Components.h` | Plain structs: `TagComponent`, `IDComponent`, `TransformComponent`, `SpriteRendererComponent`, `NativeScriptComponent` |
| Systems | `Scene::OnUpdate`, `SceneRenderer`, application layers | Functions over registry views |
| Scripts | `Scene/ScriptableEntity.h` | Per-entity C++ behaviour, run by the script system |

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

| Order | System | Runs in | Reads → writes |
| --- | --- | --- | --- |
| 1 | Native scripts | `Scene::OnUpdate` | `NativeScriptComponent` → whatever scripts touch |
| 2 | Application systems | The layer, after `Scene::OnUpdate` | Its own components. The editor's `UpdateGalaxy` reads `GalaxyStar` and writes `TransformComponent` + `SpriteRendererComponent` |
| 3 | Sprite submission | `SceneRenderer::RenderScene` | `TransformComponent` + `SpriteRendererComponent` → `Renderer2D` |
| — | GPU | `Renderer::RenderFrame` (application loop) | Records and presents everything submitted |

New engine systems go into `Scene::OnUpdate` in a deliberate position. Gameplay systems belong to the application and run after it.

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
- Don't add or remove `NativeScriptComponent`s from inside a script's `OnCreate`/`OnUpdate`. The script system is iterating over them at that point.

## Adding a feature

1. **Component**: add a plain struct to `Components.h` (engine) or to your application (game-only data, like `GalaxyStar`).
2. **System**: write a function over a view. Engine systems are called from `Scene::OnUpdate`; application systems from your layer's `OnUpdate`.
3. **Rendering**: submit through `Renderer2D` from a render system; never record Vulkan commands in a system.
4. **Editor**: show or edit the component in an editor window.

## Known gaps

- Scripts get a delta time of `0.0f`; the application loop doesn't pass frame time to `Scene::OnUpdate` yet.
- `SceneRenderer::RenderScene` uses an owning group, not a view.
- `Scene::m_Registry` is public.
- There is no `DestroyEntity`, no parent/child hierarchy and no scene serialization yet.
