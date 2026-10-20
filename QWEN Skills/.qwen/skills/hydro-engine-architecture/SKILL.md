---
name: "hydro-engine-architecture"
description: "Use when writing, changing or reviewing code in the Hydro Vulkan engine or Oxygen editor, so new code follows its subsystem lifecycle and naming, settings/project, resource-creation, ECS/systems and renderer architecture."
---

# Hydro engine architecture

Hydro is a Vulkan game engine (static library `Hydro`) used by applications such as the editor `Oxygen`. The repository's `docs/` folder is the source of truth:

- `docs/Architecture.md`: projects, startup/shutdown summary, source layout, projects and settings (`.hyproj`), frame order, cameras and clip space, render views, the editor's windows, Vulkan ownership, shaders, conventions, tooling, agreed changes (A1–A11), known gaps
- `docs/Subsystems.md`: vocabulary, subsystem lifecycle rules, startup/shutdown order, the `Time` subsystem, naming rules and the names already checked
- `docs/ResourceCreation.md`: how resources are described and created; the struct-suffix roles; the spec inventory
- `docs/SceneAndECS.md`: scenes, components, systems, frame order, cameras, play mode, scripts and the script registry, saving

Agreed direction (October 19, 2026; Roadmap "Architecture and code quality", A1–A12): tooling and CI first, then a per-frame deletion queue with RAII and `Scope` for owned GPU objects, then `SystemContext` and a stage list before physics; also a component registry, no Vulkan in `Renderer/` headers, no asset paths in the engine. New code is written the agreed way already; old code is migrated when you touch it or as part of the roadmap task.

Read the doc for the area you are changing before writing code. If what you are about to write disagrees with the docs, follow the docs, or stop and ask whether the architecture should change. Never quietly add a second way of doing something.

The wiki (`Hydro.wiki`) mirrors `docs/` (Architecture, Subsystems, Resource-Creation, Scene-and-ECS) and also has pages that live only there (Components, Coding-Conventions, Known-Issues, Roadmap, Getting-Started). When you change a doc, regenerate its wiki page from the doc (keep the "Mirrors docs/..." banner; links `X.md` -> wiki page names: `Subsystems`, `Resource-Creation`, `Scene-and-ECS`, `Architecture`). When you add or change a component, update the wiki's Components page. When you fix something, remove its row from Known-Issues (add it under "Recently fixed"), update its Roadmap status, and close or comment on its GitHub issue.

`QWEN Skills/` in the repository holds the same rules for Qwen Code on a local model: a copy of this skill, a short C++ rules file and a `hydro-quick-edit` skill for small tasks. When a rule here changes, update those copies too (see `QWEN Skills/INSTALL.md`).

## 0. Vocabulary

| Term | What it is | Lifetime |
| --- | --- | --- |
| Application | Owns subsystems and the `LayerStack`, runs the loop | `Init` -> `Run` -> `Shutdown`, called by `main` |
| Subsystem | Engine-wide service, one per application (`Log`, `Time`, `EventDispatcher`, `Window`, `Renderer`) | `Init` / `Shutdown`, called only by `Application` |
| Part | Piece a subsystem owns and starts itself (`VulkanRendererContext`, `VulkanFrameContext`, `VulkanAllocator`, `Renderer2D`, `Renderer3D`) | `Init` / `Shutdown`, called by its owning subsystem |
| Layer | Application feature in the `LayerStack` | `OnAttach` / `OnDetach` |
| System | ECS function over a `registry.view<...>()`, every frame | none |
| Resource | GPU/engine object made from a `Specification` (`VulkanImage`, `RenderView`) | constructor / destructor (RAII) |
| Hook | Virtual the engine calls on client code, named `On...` | called by its owner |

"System" always means an ECS system. An engine service is a **subsystem**. `Project` is data the application opens and closes, not a subsystem. `ScriptRegistry` is a lookup table, not a subsystem.

## 1. Subsystems: explicit startup and shutdown

- Lifecycle functions are spelled exactly `Init` and `Shutdown` (not `Create`, `Startup`, `Initialize`, `Destroy`, `ShutDown`).
- `Init` takes its dependencies as parameters (`Renderer::Init(Window&)`) plus a `Specification` if configurable (`Window::Init(const WindowSpecification&)`). It never reaches for a subsystem the caller didn't start first.
- `Init` asserts it hasn't run (`HY_CORE_ASSERT`, `Core/Assert.h`). `Shutdown` releases everything `Init` acquired and is a no-op if `Init` didn't run.
- Constructors and destructors do no work: no OS/GPU handles, no logging, no calls into other subsystems. A destructor may assert that `Shutdown` ran; it never cleans up for it.
- `Application` is the only caller of a subsystem's `Init`/`Shutdown`. `Shutdown` runs in exactly the reverse order of `Init`, with matching numbered comments.
- A subsystem starts its parts in its own `Init` and stops them in reverse in its own `Shutdown`.
- Static subsystems keep their state behind one `s_Data`/`s_Instance` pointer created in `Init` and deleted in `Shutdown`.
- Hooks are named `On...`; actions are not (`EditorCamera::Update`, `Scene::RemoveScript`, `PlayScene`).

Current order (`main`: `CreateApplication()` -> `Init()` -> `Run()` -> `Shutdown()` -> `delete`):

| # | Init | Shutdown (reverse) |
| --- | --- | --- |
| 1 | `Log::Init` (terminal, `logs/*.log`, in-memory history for the editor Console) | `Log::Shutdown` (last) |
| 2 | `Time::Init` | `Time::Shutdown` |
| 3 | `EventDispatcher::Init` | `EventDispatcher::Shutdown` |
| 4 | `Window::Create()` + `Window::Init(spec.MainWindow)` | `Window::Shutdown` + reset |
| 5 | `Renderer::Init(window)` (context, swapchain, frames, allocator, `Renderer2D`, `Renderer3D`) | `Renderer::Shutdown` (reverse) |
| 6 | push `ImGuiLayer` overlay | `Renderer::WaitIdle`, `LayerStack::Clear()` |
| 7 | client `OnInit` (registers scripts, then pushes layers) | client `OnShutdown` (first) |

Layers release what `OnAttach` created in `OnDetach`, not in their destructor (scenes, `RenderView`s, windows). Whoever opens a project closes it (`Project::Close()`; the editor does it in `OnDetach`).

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
- Platform implementations: `<Platform><Name>` (`GlfwWindow`, `VulkanRendererContext`; per-OS files in `Platform/<OS>/` named `<OS><Name>`, e.g. `MacOSFileDialogs.cpp`). Per-scene counterparts: `Scene...`/`...Scene` (`SceneRenderer`, `SceneCamera`, a future `PhysicsScene`).
- Planned positions: `JobScheduler` after `Time`; `AssetManager` after `Renderer`; `AudioEngine`, `ScriptEngine` after `AssetManager`; `PhysicsEngine` after `ScriptEngine`; all before the client's `OnInit`.
- Renaming a file: update every include and delete the old file in the same change. Don't leave forwarding stubs; if you can't delete a file yourself, list it under Known gaps and tell the user to delete it.

## 2. Settings, capabilities and the project file

- Struct roles: `Specification` (what a resource/subsystem is; fixed), `Properties` (how a resource is used), `Info` (output of creation), `Settings` (live options of a subsystem, changed while running via `Get<...>Settings`/`Set<...>Settings`, applied from the next frame), `Capabilities` (what the hardware supports, read-only, found at `Init`), `Config` (saved to disk).
- `RendererSettings` (`Renderer/RendererSettings.h`): `ClearColor`, `VSync` (recreates the swapchain), `Wireframe` (rebuilds the 2D pipeline; only if `RendererCapabilities::Wireframe`), `ShaderHotReload`. A new renderer option goes here, is read by the renderer each frame or applied in `Renderer::SetSettings`, is serialized in `ProjectSerializer`, and gets a control in Oxygen's `RenderSettingsWindow`.
- Vsync is a swapchain setting, never a window one (there is no GL context).
- The project file is `.hyproj` YAML (`ProjectConfig`: `Name`, `AssetDirectory` relative to the project folder, `StartScene` relative to the asset folder, `Renderer`). `ProjectSerializer` must keep defaults for missing keys so old files load. glm types use `Utils/YAMLConversions.h`.
- **Reading YAML:** a missing key gives an invalid node, and asking it anything except whether it exists throws `YAML::InvalidNode`. Check first (`if (auto child = node[key])` or `child.IsDefined()`), then call `IsMap()` / `IsSequence()` / `as<T>(fallback)`. Wrap a whole-file read in `try { } catch (const YAML::Exception&)` and fall back to defaults instead of crashing.
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
- Resources are immutable after creation. Recreate them, or use a dedicated rebuild such as `Resize` / `RenderView::SetSize`.
- Put "what it is" in `Specification` and "how it is used" in `Properties`.
- The created object owns its handles. Destructor frees them, copy constructor and copy assignment are deleted. Destroy it only after the GPU is done with it (frame fence, or `Renderer::WaitIdle()`). Don't add new manual `Destroy()` methods; six old wrappers still have one (`VulkanVertexBuffer`, `VulkanIndexBuffer`, `VulkanUniformBuffer`, `VulkanDescriptorBuilder`, `VulkanPipeline`, `VullkanTexture`): migrate them when you touch them, in the order buffers -> pipeline/descriptors -> textures (with VMA, #34).
- **Ownership is `Scope`.** The single owner of a GPU object holds it in a `Scope<T>` and hands it out as `T&` / `const T&`. `Ref<T>` only for objects several owners really share. Don't add new `Ref<Vulkan...>` members.
- **Deletion queue (planned, A3).** Once `Renderer::SubmitResourceFree(std::function<void()>)` exists, destructors hand their handles to it (freed after that frame's fence, and at `Renderer::Shutdown`), and code must not call `Renderer::WaitIdle()` mid-frame to free things. Until then, wait as before.
- RAII is for resources only. Subsystems and parts use explicit `Init`/`Shutdown`. One-per-application or owned-and-ordered by a subsystem means subsystem/part; many instances from a spec means resource.
- After adding or changing a spec, settings or config struct, update the inventory table in `docs/ResourceCreation.md`.

## 4. Scene, ECS and systems

- `Scene` owns the `entt::registry`. `Entity` is a handle (`entt::entity` + `Scene*`): copy it, store it by value. Every entity made with `CreateEntity` has `TagComponent` + `IDComponent` (UUID); only those are listed, saved and copied on Play.
- **Components are data.** Plain structs with defaults and at most small pure helpers (`GetTransform()`, `SceneCamera::GetProjection(aspect)`). No virtual functions, no update logic, no pointers or references to other components or entities; refer to entities by `Entity` or `UUID`. Engine components live in `Scene/Components/Components.h` (`NativeScriptComponent` in `Scene/ScriptableEntity.h`); game-only components live in the application.
- A new engine component is also added to: `EntitySerializer` (both directions, defaults for missing keys), `CopyableComponents` in `Components.h` (so play mode copies it), the inspector (`InspectorWindow::DrawComponents` and **Add Component**), and the wiki's Components page.
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
- **Planned (A5): `SystemContext`.** `struct SystemContext { float DeltaTime; float FixedDeltaTime; uint64_t FrameIndex; bool IsPlaying; };` and systems shaped `void UpdatePhysics(Scene&, const SystemContext&)`, run by `Scene` from one ordered stage list: Scripts -> Physics -> Transforms -> Render submission. Once it exists, new systems use that signature and a stage, not a new parameter list; before it exists, keep `(entt::registry&, float deltaTime)` and don't invent another shape. Physics (P1) waits for it.
- **Planned (A6): component registry.** One registration per component (name, serialize/deserialize, inspector draw) that saving, `Scene::Copy` and the inspector loop over. Until it lands, follow the four-place list above.
- Frame order, fixed and deliberate:
  1. native scripts (`Scene::OnUpdateRuntime`, play mode only)
  2. engine systems (`Scene::OnUpdateEditor`, which `OnUpdateRuntime` also calls)
  3. application systems (in the layer, after the scene update; gameplay systems run only while playing)
  4. render submission (`SceneRenderer::RenderEditor(view, editorCamera)` / `RenderRuntime(view)` -> `Renderer2D::BeginScene(view, ...)` + `DrawQuad` + `EndScene`)
  5. GPU recording (`Renderer::RenderFrame`, run by the application loop)

  Engine systems that must also run while editing go in `OnUpdateEditor`; game-only engine systems go in `OnUpdateRuntime` after the scripts. Gameplay systems belong to the application.
- **Play mode** (Oxygen): Play runs `Scene::Copy(editorScene)`; Stop throws the copy away. Saving always writes the editor scene. Raw registry entities (no `IDComponent`) aren't copied; the application copies its own.
- **Cameras**: `CameraComponent` holds a `SceneCamera` (projection data only; position from the transform, aspect from `Scene::OnViewportResize`). The game draws through the first `Primary` camera. Every projection is built with `Camera::Perspective` / `Camera::Orthographic` (zero-to-one depth, Y flip); never call `glm::perspective`/`glm::ortho` directly. Geometry is counter-clockwise; world units, not pixels.
- Systems never call Vulkan. Render systems only submit to the renderer.
- Don't keep a `T&` from `GetComponent<T>()` across code that adds or removes components; EnTT moves storage.
- **Scripts** are for one-off per-entity behaviour: derive from `ScriptableEntity`, register the type once before scenes load (`ScriptRegistry::Register<T>("Name")` in `OnInit`), and bind with `AddComponent<NativeScriptComponent>().Bind<T>()` or `.Bind("Name")`. One `NativeScriptComponent` per entity holds any number of scripts. Registered names are saved with scenes; keep them stable. Remove scripts with `Scene::RemoveScript` / `RemoveScripts` (they call `OnDestroy`). If the same logic applies to many entities, write a system instead. Never store `this` or stack addresses in components, and don't add/remove script components or destroy entities from inside a script callback.

## 5. Renderer and frame

- Headers under `Renderer/` must not include `vulkan.h` or other Vulkan headers (A7: `Renderer.h`, `Renderer2D.h`, `RenderView.h` still do; `RenderView` will return an opaque `ImTextureID`). Don't add more.
- The engine loads no files from the application's `assets/` folder (A8: `Renderer2D` still loads `assets/shaders/vertex.vert` and `fragment.frag`; its white texture is made in code). Default textures are made in code; engine shaders come from an engine resources folder or shader library.
- Layering: `Core` <- `Events` <- `Platform/{Windows,Vulkan}` <- `Renderer` <- `Scene` / `ImGUI` / `Project` <- applications. `Utils` (YAML conversions, dialogs) depends on Core only. Applications include `Hydro.h`, not `Platform/Vulkan` headers; editor UI asks `Renderer::GetCapabilities()` instead of querying the Vulkan device.
- One command buffer per frame. `Renderer::RenderFrame` waits for the frame fence, acquires, records the offscreen passes (`Renderer2D::Render`: one pass per `RenderView` begun this frame, cleared to `RendererSettings::ClearColor`), then the swapchain pass (ImGui via the present-pass callback), then submits and presents. New passes record into the frame's command buffer (`Renderer::GetCurrentCommandBuffer()` or the one passed in); don't add a separate per-frame submit.
- **Textures**: `Texture2D` (`Renderer/Texture2D.h`, no Vulkan in the header) is made from `ImageSpecification` + `TextureProperties` or `Texture2D::LoadFromFile(path)` (shared by file, `nullptr` on failure), held in `Ref<Texture2D>` because many sprites share one. Draw with `Renderer2D::DrawQuad(transform, texture, tilingFactor, tint)`; a draw call has 32 texture slots (slot 0 = white) and `Renderer2D` starts a new draw when they run out, keeping draw order. Textures a frame samples are kept alive until its fence, so dropping a `Ref` mid-frame is safe. **Until the asset system** (AS1–AS5, steps in `docs/Architecture.md` "Planned: asset system"), components hold `Ref<Texture2D>` and scenes save the file path relative to the asset folder; mark such code `TODO(assets)` and don't add a second way to reference assets.
- **Render views**: a `RenderView` (from a `RenderViewSpecification`) is one picture of a scene: its own target, camera (uniform buffer) and quad batches and texture sets. Draw into it with `Renderer2D::BeginScene(view, ...)` ... `DrawQuad` ... `EndScene()`; show it with `ImGui::Image(view.GetImGuiTexture(), ...)`; resize with `view.SetSize`. The application creates views after `Renderer::Init` and destroys them in `OnDetach`. Oxygen has two (Editor, Game) and only begins visible ones.
- `EditorCamera` options are `EditorCameraSettings` (field of view, clip planes, orbit/pan/zoom speed), passed with `EditorCamera::SetSettings`.
- Ownership: `VulkanRendererContext` (instance, surface, device), `VulkanDevice` (queues, private upload pool behind `SubmitImmediate`), `VulkanSwapChain` (presentation, present mode), `VulkanFrameContext` (per-frame command buffer, fence, semaphore), `Renderer` (runs the frame; `Init`s and `Shutdown`s all of these; owns `RendererSettings` and `RendererCapabilities`), `Renderer2D` (quad pipeline, shader, shared index buffer), `RenderView` (target, camera buffers, batches).
- Pipeline rebuilds (wireframe toggle, shader hot reload) go through `Renderer2D::RebuildQuadPipeline`: build the new one first, keep the old one on failure, `Renderer::WaitIdle()`, then swap.
- Anything the CPU writes per frame exists once per frame in flight (`MAX_FRAMES_IN_FLIGHT`), indexed by `Renderer::GetRenderFrame()`.
- Window resize rebuilds only the swapchain. Render views follow the windows that show them.
- Backend code reaches the device with `Renderer::GetVulkanDevice()` (`Renderer::GetRendererContext()` only for the instance and surface; nothing in `VulkanRendererContext` is static); GPU limits come from `GetPhysicalDevice().GetProperties()`. One-off uploads go through `VulkanDevice::SubmitImmediate([&](VkCommandBuffer cmd) { ... })` (upload pool, own fence, blocks the CPU: loading only, never per-frame work); nothing else touches the upload pool.

## 6. Editor (Oxygen) conventions

- Dockable ImGui windows are `<Name>Window` classes in `Oxygen/src/EditorWindows` with `OnImguiRender(...)` and `bool& IsOpen()`, listed in the View menu. Sub-sections drawn inside another window are `<Name>Panel`.
- Windows don't save files themselves: they expose a callback (`OnSaveProject`, `OnOpenScene`) that `EditorLayer` sets.
- `EditorLayer` owns the editor scene, the runtime copy and the active scene. Edits and saves go to the editor scene; the windows show the active scene.
- Inspector values are typed input fields (`InputFloatField`, `DrawVec3Control`), not drag bars; clamp when the field is left, not while typing.
- **Look.** The editor's look is Oxygen-only (`EditorStyle`, `EditorSettings`, `EditorChrome`); the engine never reads it. The Hydro themes (Hydro, Abyss, Glacier) are built from one accent colour. Don't hard-code UI colours: use `ImGui::GetStyle().Colors[...]`, `EditorStyle::GetHighlightColor()` for accents and `PlayModeSettings::TintColor` for play mode, and keep text readable on the light theme. Decorations a theme can't express (menu-bar droplet and waterline, icon buttons, play-mode tint, outlines) go in `EditorChrome` as static functions.
- **Preferences.** A new editor preference is a field in `EditorSettings` (or one of its sub-structs) with a default, written and read in `EditorSettingsSerializer` (reads keep the default when the key is missing), and a row on the right page of `EditorSettingsWindow`. The editor reads it live every frame or applies it where it changes. Bump `EditorSettingsSerializer::Version` only when old values must be ignored. The session part (last project/scene) is written with `SaveSession`, never with a full `Save`.
- **Fonts.** The application lists its fonts in `ApplicationSpecification::ImGui` (`ImGuiLayerSpecification`); find one with `ImGuiLayer::GetFont("Bold")` and handle `nullptr`. The atlas is built once at startup, so font-size changes need a restart; `EditorStyle::FontScale` applies at once.
- Shared table helpers are in `StatsUI`. Paths come from `EditorPaths` (project-aware), never hard-coded.

## 7. Code conventions

- **Namespaces.** All engine and editor code lives in `namespace Hydro`. Don't create other namespaces, nested namespaces (`Hydro::Something`) or anonymous `namespace { }` blocks. File-local helpers are `static` functions (and `const` / `constexpr` variables) inside `Hydro`. A group of related helpers is a class with static functions (`EditorPaths`, `AssetDragDrop`, `StatsUI`, like `Input` or `Renderer2D`). The only exceptions are specializations a library requires in its own namespace: `std::hash<>` in `namespace std` and `YAML::convert<>` in `namespace YAML`. Example code and scripts in Oxygen are in `Hydro` too.
- No logical-not operator anywhere: `== false` for booleans, `== nullptr` for pointers. This includes new code and code you edit.
- **Always braces.** Every `if`, `else`, `else if`, `for`, `while` and `do` body is in `{ }`, each brace on its own line, even for a single statement (`if (x) return;` is not allowed). Only `case X: return Y;` lines in a `switch` may stay on one line. `.clang-format` has `InsertBraces: true`. Add braces to older code when you edit it.
- `Ref<T>`/`Scope<T>` with `CreateRef`/`CreateScope`.
- **Naming.** Members `m_PascalCase` (never `m_camelCase`: `m_renderPass`, `m_format` are being fixed, A10), statics `s_PascalCase`, `PascalCase` types, methods and spec/plain-struct fields (`PipelineSpecification::Shader`, not `shader`). `enum class Name : type`, never `typedef enum class`. Check spelling (`VullkanTexture`, `HierachyWindow`, `heigth`, `GetPipeLine` are known misspellings being renamed; don't copy them into new names).
- **Signatures.** One-argument constructors are `explicit`. Take a `Ref<T>` you don't keep as `const Ref<T>&` (never non-const `Ref<T>&`); prefer `T&` when you only use the object. Getters get `const` overloads. Mark parameters that must exist but aren't used `[[maybe_unused]]` instead of deleting their names.
- `VK_CHECK_RESULT(...)` around Vulkan calls that return `VkResult`; `HY_CORE_ASSERT(condition, message)` for debug checks.
- **Includes.** Forward slashes. Engine headers from `src` (`"Hydro/Scene/Entity.h"`), never `../`; a header in the same folder may use its bare name. Vendored libraries through their include directory (`<entt.hpp>`, `<stb_image.h>`, `<tiny_obj_loader.h>`, `<imgui.h>`), never a path into `vendor/`; a new vendored library gets an `IncludeDir` in `Dependencies.lua` and is listed in `ExternalIncludeDirs { ... }` in the project's `premake5.lua` (the bundled premake is 5.0.0-beta1, which has no `externalincludedirs`; the helper uses MSVC's `/external:I` there). Every header has `#pragma once`, includes what it uses, and never includes `hypch.h`. Don't bind temporaries to non-const references (MSVC accepts it; other compilers don't).
- **Tooling.** Format with the root `.clang-format` (tabs, Allman, indented `namespace Hydro`, `public:` at class level, `Type* name`, sorted includes, no column limit); follow `.editorconfig` (CRLF, tabs, UTF-8; 2 spaces for YAML/`.hyscene`/`.hyprefab`/`.hyproj`). Hydro and Oxygen build at `/W4`; vendored code is external with warnings off. New code must not add warnings (`FatalWarnings` in the root `premake5.lua` will make them errors). Don't reformat whole files you aren't otherwise changing.
- Keep CRLF line endings in the working copy (`.gitattributes` stores LF in git).

## 8. Before finishing

Check the change against this list:

- [ ] Everything is in `namespace Hydro`: no new namespaces, no nested or anonymous ones; file-local helpers are `static`
- [ ] New engine services are subsystems: named by the table (no `...System`), `Init`/`Shutdown` only from `Application` at mirrored positions, no work in constructor/destructor, `Init` asserts against double init
- [ ] New parts are started/stopped by their owning subsystem, in reverse order
- [ ] Layers release in `OnDetach` what they created in `OnAttach`
- [ ] Timing comes from `Time`; new timed work uses a `FramePhase`
- [ ] Live options are `<Name>Settings` with `Get`/`Set`; anything to keep is in `ProjectConfig` and serialized with defaults for missing keys
- [ ] New resources take a `Specification`/`Properties` struct, own their handles, are non-copyable and are held in a `Scope<T>` by their owner (no new `Destroy()`, no new `Ref<Vulkan...>` members)
- [ ] Engine-level specs and `Renderer/` headers contain no Vulkan types; the engine loads nothing from `assets/`
- [ ] New behaviour is a system over a view (or a script, if it's truly per-entity), in a deliberate place in the frame order
- [ ] Components stay plain data, with no pointers to other components or entities; new ones are saved, in `CopyableComponents`, and in the inspector
- [ ] Projections come from `Camera::Perspective` / `Camera::Orthographic`
- [ ] GPU work is recorded in the frame's command buffer; per-frame resources exist per frame in flight
- [ ] No `!` operators; every `if` / `else` / loop body in `{ }`; forward-slash includes from `src` (no `../`, no `vendor/` paths); `#pragma once`
- [ ] Names follow the rules (`m_PascalCase`, PascalCase fields, `enum class`, `explicit`, `const` getters, `[[maybe_unused]]`); no new warnings at `/W4`; formatted with `.clang-format`
- [ ] `docs/` and the mirrored wiki pages updated if the change adds a subsystem, setting, spec, component, system or pass, or changes ownership, startup/shutdown or frame order. Wiki-only pages (Components, Coding-Conventions, Known-Issues, Roadmap) updated too; tick the Roadmap task boxes the change completes. If it fixes something under "Known gaps" or in Known-Issues, remove it there, update the Roadmap status, and close or comment on the GitHub issue.

In the summary, say which rule each part of the change follows. If something had to break a rule, list it as a known gap in the docs instead of hiding it.