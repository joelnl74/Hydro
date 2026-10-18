---
name: "hydro-engine-architecture"
description: "Use when writing, changing or reviewing code in the Hydro Vulkan engine or Oxygen editor, so new code follows its subsystem lifecycle and naming, settings/project, resource-creation, ECS/systems and renderer architecture."
---

# Hydro engine architecture

Hydro is a Vulkan game engine (static library `Hydro`) used by applications such as the editor `Oxygen`. The repository's `docs/` folder is the source of truth:

- `docs/Architecture.md`: projects, startup/shutdown summary, source layout, projects and settings (`.hyproj`), frame order, Vulkan ownership, shaders, conventions, known gaps
- `docs/Subsystems.md`: vocabulary, subsystem lifecycle rules, startup/shutdown order, the `Time` subsystem, naming rules and the names already checked
- `docs/ResourceCreation.md`: how resources are described and created; the struct-suffix roles
- `docs/SceneAndECS.md`: scenes, components, systems, scripts

Read the doc for the area you are changing before writing code. If what you are about to write disagrees with the docs, follow the docs, or stop and ask whether the architecture should change. Never quietly add a second way of doing something.

The wiki (`Hydro.wiki`) mirrors `docs/` (Architecture, Subsystems, Resource-Creation, Scene-and-ECS) and also has pages that live only there (Coding-Conventions, Known-Issues, Roadmap). When you change a doc, regenerate its wiki page from the doc (keep the "Mirrors docs/..." banner; links `X.md` -> wiki page names: `Subsystems`, `Resource-Creation`, `Scene-and-ECS`, `Architecture`). When you fix something, remove its row from Known-Issues (add it under "Recently fixed") and update its Roadmap status.

## 0. Vocabulary

| Term | What it is | Lifetime |
| --- | --- | --- |
| Application | Owns subsystems and the `LayerStack`, runs the loop | `Init` -> `Run` -> `Shutdown`, called by `main` |
| Subsystem | Engine-wide service, one per application (`Log`, `Time`, `EventDispatcher`, `Window`, `Renderer`) | `Init` / `Shutdown`, called only by `Application` |
| Part | Piece a subsystem owns and starts itself (`VulkanRendererContext`, `VulkanFrameContext`, `VulkanAllocator`, `Renderer2D`, `Renderer3D`) | `Init` / `Shutdown`, called by its owning subsystem |
| Layer | Application feature in the `LayerStack` | `OnAttach` / `OnDetach` |
| System | ECS function over a `registry.view<...>()`, every frame | none |
| Resource | GPU/engine object made from a `Specification` | constructor / destructor (RAII) |
| Hook | Virtual the engine calls on client code, named `On...` | called by its owner |

"System" always means an ECS system. An engine service is a **subsystem**. `Project` is data the application opens and closes, not a subsystem.

## 1. Subsystems: explicit startup and shutdown

- Lifecycle functions are spelled exactly `Init` and `Shutdown` (not `Create`, `Startup`, `Initialize`, `Destroy`, `ShutDown`).
- `Init` takes its dependencies as parameters (`Renderer::Init(Window&)`) plus a `Specification` if configurable (`Window::Init(const WindowSpecification&)`). It never reaches for a subsystem the caller didn't start first.
- `Init` asserts it hasn't run (`HY_CORE_ASSERT`, `Core/Assert.h`). `Shutdown` releases everything `Init` acquired and is a no-op if `Init` didn't run.
- Constructors and destructors do no work: no OS/GPU handles, no logging, no calls into other subsystems. A destructor may assert that `Shutdown` ran; it never cleans up for it.
- `Application` is the only caller of a subsystem's `Init`/`Shutdown`. `Shutdown` runs in exactly the reverse order of `Init`, with matching numbered comments.
- A subsystem starts its parts in its own `Init` and stops them in reverse in its own `Shutdown`.
- Static subsystems keep their state behind one `s_Data`/`s_Instance` pointer created in `Init` and deleted in `Shutdown`.
- Hooks are named `On...`; actions are not.

Current order (`main`: `CreateApplication()` -> `Init()` -> `Run()` -> `Shutdown()` -> `delete`):

| # | Init | Shutdown (reverse) |
| --- | --- | --- |
| 1 | `Log::Init` | `Log::Shutdown` (last) |
| 2 | `Time::Init` | `Time::Shutdown` |
| 3 | `EventDispatcher::Init` | `EventDispatcher::Shutdown` |
| 4 | `Window::Create()` + `Window::Init(spec.MainWindow)` | `Window::Shutdown` + reset |
| 5 | `Renderer::Init(window)` (context, swapchain, frames, allocator, `Renderer2D`, `Renderer3D`) | `Renderer::Shutdown` (reverse) |
| 6 | push `ImGuiLayer` overlay | `Renderer::WaitIdle`, `LayerStack::Clear()` |
| 7 | client `OnInit` | client `OnShutdown` (first) |

Layers release what `OnAttach` created in `OnDetach`, not in their destructor. Whoever opens a project closes it (`Project::Close()`; the editor does it in `OnDetach`).

### Time

`Time` (`Core/Time.h`) is the frame clock and CPU frame timings. Never keep your own clock:

- `Time::GetDeltaTime()` (seconds, clamped to `Time::MaxDeltaTime`), `Time::GetFrameCount()`, `Time::GetUptime()`
- `Time::GetFrameTimings()` (last complete frame): `GetPhaseMs(FramePhase::...)`, `FrameMs`, `DeltaMs` (unclamped)
- `Application::Run` drives `BeginFrame`/`EndFrame` and phases; `Renderer::RenderFrame` reports `Render` and `Present`. New timed work gets a `FramePhase`.
- `Layer::OnUpdate(deltaTime)` receives `Time::GetDeltaTime()`; pass it on to systems and scripts.

### Naming subsystems

| Kind | Suffix | Use when | Names |
| --- | --- | --- | --- |
| Domain | none | Facade over one concept | `Log`, `Time`, `Input`, `Window` |
| Doer | agent noun (`-er`/`-or`) | Performs one job on what is submitted | `Renderer`, `EventDispatcher` |
| Runtime | `Engine` | Steps a simulation or hosts a foreign runtime each frame | `AudioEngine`, `ScriptEngine`, `PhysicsEngine` |
| Registry | `Manager` | Owns a collection looked up by handle/ID | `AssetManager` |

- Never `...System` (reserved for ECS). Not `Service`, `Handler`, `Helper`, `Controller`, `Module`.
- `Renderer` stays `Renderer` (a doer, not `RenderEngine`). `LayerController` was renamed `LayerStack`. The full verdict list is in `docs/Subsystems.md` "Names we checked"; add a row when you name or rename something.
- Platform implementations: `<Platform><Name>` (`WindowsWindow`, `VulkanRendererContext`). Per-scene counterparts: `Scene...`/`...Scene` (`SceneRenderer`).
- Planned positions: `JobScheduler` after `Time`; `AssetManager` after `Renderer`; `AudioEngine`, `ScriptEngine` after `AssetManager`; `PhysicsEngine` after `ScriptEngine`; all before the client's `OnInit`.
- If you rename a file but can't delete the old one, leave a forwarding stub that includes the new header, list it under Known gaps, and tell the user to delete it.

## 2. Settings, capabilities and the project file

- Struct roles: `Specification` (what a resource/subsystem is; fixed), `Properties` (how a resource is used), `Info` (output of creation), `Settings` (live options of a subsystem, changed while running via `Get<...>Settings`/`Set<...>Settings`, applied from the next frame), `Capabilities` (what the hardware supports, read-only, found at `Init`), `Config` (saved to disk).
- `RendererSettings` (`Renderer/RendererSettings.h`): `ClearColor`, `VSync` (recreates the swapchain), `Wireframe` (rebuilds the 2D pipeline; only if `RendererCapabilities::Wireframe`), `ShaderHotReload`. A new renderer option goes here, is read by the renderer each frame or applied in `Renderer::SetSettings`, is serialized in `ProjectSerializer`, and gets a control in Oxygen's `RenderSettingsWindow`.
- Vsync is a swapchain setting, never a window one (there is no GL context).
- The project file is `.hyproj` YAML (`ProjectConfig`: `Name`, `AssetDirectory` relative to the project folder, `StartScene` relative to the asset folder, `Renderer`). `ProjectSerializer` must keep defaults for missing keys so old files load. glm types use `Utils/YAMLConversions.h`.
- `Project` never calls other subsystems. Whoever opens a project applies its settings (`Renderer::SetSettings(config.Renderer)`); when saving, copy the live settings back (`config.Renderer = Renderer::GetSettings()`), then `Project::SaveActive()`.
- Anything a user should keep between runs belongs in `ProjectConfig` (or a scene), not in static defaults or editor-only state.

## 3. Resource creation: Parameter Object pattern

Every resource is created from a plain struct passed to its constructor. It is never configured with setters afterwards.

```cpp
ImageSpecification spec;
spec.Width = 1280;
spec.Height = 720;
spec.Usage = ImageUsage::Attachment;
Scope<VulkanImage> target = CreateScope<VulkanImage>(spec);
```

Call it the **Parameter Object** (resource descriptor / configuration object) pattern. It is *not* the DDD Specification pattern.

- Specs are plain aggregates: public fields, every field defaulted, no logic. A default-constructed spec must create something valid.
- The constructor takes `const FooSpecification&` and stores a copy if needed (`GetSpecification()`).
- Specs in `Renderer/` use engine enums only. Vulkan types appear only in specs under `Platform/Vulkan/`. Convert engine enums to Vulkan in one static helper.
- Resources are immutable after creation. Recreate them, or use a dedicated rebuild such as `Resize`.
- Put "what it is" in `Specification` and "how it is used" in `Properties`.
- The created object owns its handles. Destructor frees them, copy constructor and copy assignment are deleted, and it is held in `Scope`/`Ref`. Destroy it only after the GPU is done with it (frame fence, or `Renderer::WaitIdle()`). Don't add new manual `Destroy()` methods; migrate old ones when you touch them.
- RAII is for resources only. Subsystems and parts use explicit `Init`/`Shutdown`. One-per-application or owned-and-ordered by a subsystem means subsystem/part; many instances from a spec means resource.
- After adding or changing a spec, settings or config struct, update the inventory table in `docs/ResourceCreation.md`.

## 4. Scene, ECS and systems

- `Scene` owns the `entt::registry`. `Entity` is a handle (`entt::entity` + `Scene*`): copy it, store it by value. Every entity has `TagComponent` + `IDComponent` (UUID).
- **Components are data.** Plain structs with defaults and at most small pure helpers (`GetTransform()`). No virtual functions, no update logic, no pointers or references to other components or entities; refer to entities by `Entity` or `UUID`. Engine components live in `Scene/Components/Components.h`; game-only components live in the application. A new engine component is also added to `EntitySerializer`.
- **Behaviour lives in systems.** A system is a function over a `registry.view<...>()` that does one job, taking `deltaTime` as a parameter:

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

- Use views. Don't add owning `group<...>()`s: only one may own a component type.
- Systems run in a fixed, deliberate order:
  1. native scripts (`Scene::OnUpdate`)
  2. application systems (in the layer, after `Scene::OnUpdate`)
  3. render submission (`SceneRenderer::RenderScene` -> `Renderer2D::DrawQuad`)
  4. GPU recording (`Renderer::RenderFrame`, run by the application loop)

  Engine systems go into `Scene::OnUpdate`; gameplay systems belong to the application.
- Systems never call Vulkan. Render systems only submit to the renderer.
- Don't keep a `T&` from `GetComponent<T>()` across code that adds or removes components; EnTT moves storage.
- **Scripts** are for one-off per-entity behaviour: derive from `ScriptableEntity`, attach with `entity.AddComponent<NativeScriptComponent>().Bind<MyScript>()`. If the same logic applies to many entities, write a system instead. Never store `this` or stack addresses in components, and don't add or remove `NativeScriptComponent`s from inside a script callback.

## 5. Renderer and frame

- Layering: `Core` <- `Events` <- `Platform/{Windows,Vulkan}` <- `Renderer` <- `Scene` / `ImGUI` / `Project` <- applications. `Utils` (YAML conversions, dialogs) depends on Core only. Applications include `Hydro.h`, not `Platform/Vulkan` headers; editor UI asks `Renderer::GetCapabilities()` instead of querying the Vulkan device.
- One command buffer per frame. `Renderer::RenderFrame` waits for the frame fence, acquires, records the offscreen passes (`Renderer2D::Render`, cleared to `RendererSettings::ClearColor`), then the swapchain pass (ImGui via the present-pass callback), then submits and presents. New passes record into the frame's command buffer (`Renderer::GetCurrentCommandBuffer()` or the one passed in); don't add a separate per-frame submit.
- Ownership: `VulkanRendererContext` (instance, surface, device), `VulkanDevice` (queues, upload pool), `VulkanSwapChain` (presentation, present mode), `VulkanFrameContext` (per-frame command buffer, fence, semaphore), `Renderer` (runs the frame; `Init`s and `Shutdown`s all of these; owns `RendererSettings` and `RendererCapabilities`).
- Pipeline rebuilds (wireframe toggle, shader hot reload) go through `Renderer2D::RebuildQuadPipeline`: build the new one first, keep the old one on failure, `Renderer::WaitIdle()`, then swap.
- Anything the CPU writes per frame exists once per frame in flight (`MAX_FRAMES_IN_FLIGHT`), indexed by `Renderer::GetRenderFrame()`.
- Window resize rebuilds only the swapchain. The editor's offscreen target follows the viewport panel (`Renderer2D::SetViewportSize`).
- One-off uploads use `VulkanCommandBuffer` (device upload pool, waits for completion).

## 6. Editor (Oxygen) conventions

- Dockable ImGui windows are `<Name>Window` classes in `Oxygen/src/EditorWindows` with `OnImguiRender(...)` and `bool& IsOpen()`, listed in the View menu. Sub-sections drawn inside another window are `<Name>Panel`.
- Windows don't save files themselves: they expose a callback (`OnSaveProject`, `OnOpenScene`) that `EditorLayer` sets.
- Shared table helpers are in `StatsUI.h`. Paths come from `EditorPaths` (project-aware), never hard-coded.

## 7. Code conventions

- No logical-not operator anywhere: `== false` for booleans, `== nullptr` for pointers. This includes new code and code you edit.
- `Ref<T>`/`Scope<T>` with `CreateRef`/`CreateScope`; members `m_`, statics `s_`, `PascalCase` types and methods.
- `VK_CHECK_RESULT(...)` around Vulkan calls that return `VkResult`; `HY_CORE_ASSERT(condition, message)` for debug checks.
- Forward-slash includes; every header has `#pragma once`, includes what it uses, and never includes `hypch.h`. Don't bind temporaries to non-const references (MSVC accepts it; other compilers don't).
- Keep the file's existing line endings (the Windows checkout uses CRLF).

## 8. Before finishing

Check the change against this list:

- [ ] New engine services are subsystems: named by the table (no `...System`), `Init`/`Shutdown` only from `Application` at mirrored positions, no work in constructor/destructor, `Init` asserts against double init
- [ ] New parts are started/stopped by their owning subsystem, in reverse order
- [ ] Layers release in `OnDetach` what they created in `OnAttach`
- [ ] Timing comes from `Time`; new timed work uses a `FramePhase`
- [ ] Live options are `<Name>Settings` with `Get`/`Set`; anything to keep is in `ProjectConfig` and serialized with defaults for missing keys
- [ ] New resources take a `Specification`/`Properties` struct, own their handles and are non-copyable
- [ ] Engine-level specs contain no Vulkan types
- [ ] New behaviour is a system over a view (or a script, if it's truly per-entity), in a deliberate place in the frame order
- [ ] Components stay plain data, with no pointers to other components or entities
- [ ] GPU work is recorded in the frame's command buffer; per-frame resources exist per frame in flight
- [ ] No `!` operators; forward-slash includes; `#pragma once`
- [ ] `docs/` and the mirrored wiki pages updated if the change adds a subsystem, setting, spec, component, system, pass, or changes ownership, startup/shutdown or frame order. If it fixes something under "Known gaps" or in the wiki's Known-Issues, remove it there and update the Roadmap status.

In the summary, say which rule each part of the change follows. If something had to break a rule, list it as a known gap in the docs instead of hiding it.