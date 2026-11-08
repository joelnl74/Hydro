---
name: "hydro-engine-architecture"
description: "Read when explaining or reviewing Hydro engine or Oxygen editor code, or checking that game code follows the engine's rules. The full architecture rules (large: load only when needed). Engine changes themselves are handed off with hydro-handoff."
---

# Hydro engine architecture

Hydro is a Vulkan game engine (static library `Hydro`) used by applications such as the editor `Oxygen`. `docs/` in the repository is the source of truth. This skill holds the rules; the docs hold the details. Don't read the docs end to end: find the file in the code map, then open only the section you need.

## Find things first (saves most of the work)

1. **`docs/CodeMap.md`**: one line per source file, and a table of which files each kind of change touches (component, system, renderer setting, editor preference, window, resource, subsystem, textures, docs). Read it before opening code.
2. Search instead of guessing (`grep -rn` over `Hydro/src`, `Oxygen/src` and `Game/src`). Unfinished work is tagged `TODO(assets)`; every doc ends with **Known gaps**.
3. Open the doc section for the area you change:
   - `docs/Subsystems.md`: vocabulary, lifecycle rules, startup/shutdown order table, `Time`, naming table and "Names we checked", planned subsystems and their positions
   - `docs/ResourceCreation.md`: struct-suffix roles, rules, ownership, spec inventory
   - `docs/SceneAndECS.md`: components, systems, frame order, cameras, play mode, scripts, saving, planned `SystemContext` and component registry
   - `docs/Architecture.md`: layout and layering, projects and settings, the frame, render views, **textures and batching**, the editor, Vulkan ownership, shaders, conventions, tooling, agreed changes A1-A12, **planned asset system**, known gaps
4. **Cloud session** (no shell on the user's computer): clone `https://github.com/joelnl74/Hydro` (public) to read and search; it only has what is pushed, so ask what is uncommitted. Edit the user's files on their computer (copy over only the files you change, guarded by modification time), never just the clone. Ask for the `Hydro` and `Hydro.wiki` folders at the start of a task that touches docs.
5. **Check** with `scripts/check.sh [files...]` (g++ `-fsyntax-only -Wall -Wextra` + glslang on the shaders; Linux). It is not a build: say an MSVC Debug build is still needed.

If what you are about to write disagrees with the docs, follow the docs or stop and ask whether the architecture should change. Never quietly add a second way of doing something.

Agreed direction (Oct 19, 2026; Roadmap A1-A12): tooling and CI first, then a per-frame deletion queue with RAII and `Scope` for owned GPU objects (done Oct 20, 2026), then `SystemContext` and a stage list before physics; also a component registry, no Vulkan in `Renderer/` headers, no asset paths in the engine. New code is written the agreed way already; old code is migrated when you touch it.

## Vocabulary

Application (owns subsystems + `LayerStack`, `Init` -> `Run` -> `Shutdown` from `main`), **subsystem** (engine-wide service: `Log`, `Time`, `EventDispatcher`, `Window`, `Renderer`; `Init`/`Shutdown` only from `Application`), **part** (owned and started by a subsystem: `VulkanRendererContext`, `VulkanFrameContext`, `VulkanAllocator`, `Renderer2D`, `Renderer3D`), **layer** (`OnAttach`/`OnDetach`), **system** (always an ECS function over a view), **resource** (made from a `Specification`, RAII), **hook** (`On...` virtual the engine calls; actions are not named `On...`: `EditorCamera::Update`, `Scene::RemoveScript`). `Project` is data, `ScriptRegistry` a lookup table: neither is a subsystem.

## Subsystems

- Lifecycle is spelled `Init` / `Shutdown` (never `Create`, `Startup`, `Initialize`, `Destroy`, `ShutDown`). `Init` takes its dependencies as parameters (`Renderer::Init(Window&)`) plus a `Specification` if configurable, never reaches for a subsystem the caller didn't start first, and asserts it hasn't run (`HY_CORE_ASSERT`); `Shutdown` releases everything and is a no-op if `Init` didn't run.
- Constructors/destructors do no work (no handles, logging or calls into other subsystems); a destructor may assert `Shutdown` ran, never clean up for it.
- Only `Application` calls subsystem `Init`/`Shutdown`; shutdown is the exact reverse with matching numbered comments. A subsystem starts its parts in its `Init` and stops them in reverse. Static subsystems keep state behind one `s_Data`/`s_Instance` pointer.
- Order and planned positions (`JobScheduler` after `Time`; `AssetManager` after `Renderer`; `AudioEngine`, `ScriptEngine` after `AssetManager`; `PhysicsEngine` after `ScriptEngine`; all before the client's `OnInit`): `docs/Subsystems.md`.
- Layers release in `OnDetach` what `OnAttach` created (scenes, `RenderView`s, windows). Whoever opens a project closes it (`Project::Close()`).
- Timing comes from `Time` (`GetDeltaTime`, `GetFrameCount`, `GetUptime`, `GetFrameTimings`); never keep your own clock; new timed work gets a `FramePhase`; pass `deltaTime` from `Layer::OnUpdate` on to systems and scripts.
- Naming: domain facade (no suffix), doer (`-er`/`-or`), runtime (`Engine`), registry (`Manager`). Never `...System`, `Service`, `Handler`, `Helper`, `Controller`, `Module`. Platform classes `<Platform><Name>`; per-scene counterparts `Scene...`/`...Scene`. Add a row to "Names we checked" when you name or rename something.
- Renaming a file: update every include and delete the old file in the same change (no forwarding stubs); if you can't delete it, list it under Known gaps and tell the user.

## Settings and the project file

- Struct roles: `Specification` (what it is, fixed), `Properties` (how it's used), `Info` (output), `Settings` (live, `Get/Set<...>Settings`, applied next frame), `Capabilities` (hardware, read-only, found at `Init`), `Config` (saved).
- A new renderer option: field in `RendererSettings`, read each frame or applied in `Renderer::SetSettings`, serialized in `ProjectSerializer`, control in `RenderSettingsWindow`. Vsync is a swapchain setting, never a window one.
- `.hyproj` is YAML (`ProjectConfig`); missing keys keep defaults; glm types through `Utils/YAMLConversions.h`. **Reading YAML**: a missing key gives an invalid node that throws on anything but an existence check, so check first (`if (auto child = node[key])`), then `as<T>(fallback)`; wrap whole-file reads in `try`/`catch (const YAML::Exception&)` and fall back to defaults.
- `Project` never calls other subsystems: the opener applies settings (`Renderer::SetSettings(config.Renderer)`) and copies live settings back before `Project::SaveActive()`. Anything a user should keep goes in `ProjectConfig` or a scene.

## Resources (Parameter Object pattern, not the DDD Specification pattern)

- Created from a plain struct passed to the constructor (`CreateScope<VulkanImage>(spec)`), never configured with setters. Specs are aggregates with every field defaulted; the default must create something valid.
- Constructor takes `const FooSpecification&`. Specs in `Renderer/` use engine enums only; Vulkan types only under `Platform/Vulkan/`; convert in one static helper.
- Immutable after creation (recreate, or a dedicated rebuild like `Resize` / `RenderView::SetSize`). The object owns its handles: destructor frees them, copies deleted. No manual `Destroy()` methods (none are left).
- **Deletion queue**: a destructor passes a lambda capturing its handles (never `this`) to `Renderer::SubmitResourceFree`, which runs it after the frames in flight (also after a swapchain rebuild and at `Renderer::Shutdown`). So a resource can be released at any point in a frame; never call `Renderer::WaitIdle()` to free something. GPU resources must be released before `Renderer::Shutdown` (layers in `OnDetach`). `VulkanShader` frees its modules at once.
- **Ownership is `Scope`**; hand out `T&` (`Renderer::GetVulkanDevice()`, `GetRendererContext()`, `GetPhysicalDevice()`, `GetVulkanSwapChain()` return references). `Ref<T>` only when several owners really share it (a `Texture2D` used by many sprites). No `Ref<Vulkan...>` members. Non-owning links are references or documented plain pointers (`PipelineSpecification::Shader` / `DescriptorSets`).
- RAII is for resources only; subsystems and parts use `Init`/`Shutdown` (one per application or owned and ordered by a subsystem = subsystem/part; many from a spec = resource). Update the inventory in `docs/ResourceCreation.md` when you add or change a spec, settings or config struct.

## Scene, ECS and systems

- `Scene` owns the `entt::registry` (old vendored EnTT: `has<T>`, no `all_of`). `Entity` is a handle (`entt::entity` + `Scene*`); store by value. Only entities from `CreateEntity` (Tag + ID) are listed, saved and copied on Play.
- **Components are data**: defaults, small pure helpers only, no virtuals, no update logic, no pointers to other components or entities (use `Entity` or `UUID`). Shared engine resources are fine (`Ref<Texture2D>`). Engine components in `Scene/Components/Components.h` (`NativeScriptComponent` in `Scene/ScriptableEntity.h`); game-only ones in the application.
- A new engine component also goes in: `EntitySerializer` (both directions, defaults), `CopyableComponents`, the inspector (`DrawComponents` + **Add Component**), the wiki Components page. (The A6 component registry will replace this list.)
- **Behaviour lives in systems**: one job, a function over `registry.view<...>()`, `(entt::registry&, float deltaTime)` until A5's `SystemContext` (`(Scene&, const SystemContext&)` + a stage: Scripts -> Physics -> Transforms -> Render submission) exists; don't invent another shape. Physics waits for A5. Views, not owning groups. Systems never call Vulkan; render systems only submit.
- Frame order: native scripts (play only) -> engine systems (`OnUpdateEditor`, also called at runtime; game-only engine systems in `OnUpdateRuntime` after scripts) -> application systems (in the layer) -> render submission (`SceneRenderer` -> `Renderer2D::BeginScene(view, ...)`/`DrawQuad`/`EndScene`) -> GPU recording (`Renderer::RenderFrame`). Details: `docs/SceneAndECS.md#frame-order`.
- Don't keep a `T&` from `GetComponent` across adds/removes. Play mode runs on `Scene::Copy`; Stop discards it; saving always writes the editor scene.
- Cameras: `CameraComponent` holds a `SceneCamera` (position from the transform, aspect from `Scene::OnViewportResize`); the game draws through the first `Primary` camera. Projections only from `Camera::Perspective` / `Camera::Orthographic` (zero-to-one depth, Y flip), never `glm::perspective`/`ortho`; counter-clockwise geometry; world units.
- Scripts (`ScriptableEntity`) only for one-off per-entity behaviour. Mark the class `HY_SCRIPT()` and public fields `HY_PROPERTY(...)`; the generator registers them under the **class name**, which scenes save, so keep it stable (until former names exist, Scripting plan SC1). Bind with `.Bind<T>()` or `.Bind("Name")`; remove with `Scene::RemoveScript(s)`. Structural changes from script callbacks are deferred to the end of the stage. Never store `this` or stack addresses in components.

## Game scripts (`Game/src`)

The agreed rules are in the wiki's [Scripting plan](https://github.com/joelnl74/Hydro/wiki/Scripting-Plan) (Nov 1, 2026; steps SC1-SC9). Today's scripts don't follow them all yet (SC8 refactors them); new game code does, using what exists so far.

- **Layers:**

  | Layer | Where | Rule |
  | --- | --- | --- |
  | Data | `Data/` (`HY_DATA` definitions and tables, files in `Assets/Data/`) | Authored content: anything to tune, translate, reference or balance, read through a property that defaults to the file |
  | State | `State/`, on the game session (SC2) | Run state; plain C++ that refers to content by row or handle, never by pointer. Never in a script class or a static |
  | Rules | `Rules/` | Pure functions: data, state and a `Random&` in, a result struct out; unit-tested; no entities, UI, effects or cameras |
  | Scripts | `Scripts/` | Orchestrate: input, physics and UI events in; rules called; state changed; views and events out |
  | Views | `Views/` or the area folder | Present state and turn UI events into intents; never change state. A plain class owned by one script by default (`Bind(Entity root)`, `Present(...)`); a view script when several scripts use it |

- **Where files go:** used by more than one area or scene → the shared folder for its kind (`Data/`, `State/`, `Rules/`, `Views/`, `Scripts/Common/`); used by one → `Scripts/<Area>/` (`Battle/`, `Field/`, `ShrineGlade/`, ...). An area never includes another area's files. Moving a file changes no saved name.
- **Reaching another object:**
  - A dependency known while authoring is a `ComponentRef<T>` / `EntityRef` property (SC4).
  - An entity known only at runtime (a collision's `other`, a ray's hit), or a sibling on the same entity, uses `TryGetComponent<T>()` (SC3).
  - Systems that shouldn't know each other use state and events.
  - No global tag lookups or `dynamic_cast` scans in new code. `FindChild(path)` is only for the inside of a view or prefab.
- **Events** are for one-off happenings (damage taken, item used), read by feedback (popups, sounds, shakes). The current truth is state, a calculation is a rule's result. Keep the queue small.
- **One script of a type per entity** (two `BattleScript`s on one entity is an error).
- **Serialized names are an API:** script classes, properties, data types and fields. Rename only with a former name (SC1).
- **Split a script when it owns several distinct responsibilities** (its description needs "and" twice). ~500 lines is a reason to look, not a rule.
- **Revisions only where a retained view needs them.** Then the fields are private and every changing method bumps the revision.
- **Naming:** state named for what it holds (`Inventory`, `Party`), rules `<Area>Rules`, view classes `<Name>View`, scripts `<Name>Script`. The engine's suffix bans apply.
- **Game data types** stay game-specific (`Game/src/Data/`): only the fields the game reads, generalised when a second use appears.

## Renderer

- Backend code (`Platform/Vulkan`, `Renderer2D`, `ImGuiLayer`) reaches the device with `Renderer::GetVulkanDevice()` (one local `VulkanDevice&` per function); `Renderer::GetRendererContext()` only for the instance and surface; nothing in `VulkanRendererContext` is static. Parts started with a device take it as an `Init` parameter. GPU limits from `GetPhysicalDevice().GetProperties()`, not `vkGetPhysicalDeviceProperties`. Work that needs the device's internals is a `VulkanDevice` function, not callers fetching its pools and queues.
- No new Vulkan includes in `Renderer/` headers (A7: `Renderer.h`, `Renderer2D.h`, `RenderView.h` still have them). Backend data goes behind an opaque struct (`RenderViewData`, `Texture2DData`).
- The engine loads nothing from the application's `assets/` (A8: only `Renderer2D`'s shaders remain). Default textures are made in code.
- Layering: `Core` <- `Events` <- `Platform/{Windows,Vulkan}` <- `Renderer` <- `Scene` / `ImGui` / `Project` <- applications; `Utils` depends on Core only. Applications include `Hydro.h`, never `Platform/Vulkan`; editor UI asks `Renderer::GetCapabilities()`.
- One command buffer per frame; record new passes into it (`Renderer::GetCurrentCommandBuffer()` or the one passed in); no separate per-frame submit. Anything the CPU writes per frame exists once per frame in flight, indexed by `Renderer::GetRenderFrame()`. One-off uploads go through `VulkanDevice::SubmitImmediate([&](VkCommandBuffer cmd) { ... })` (own fence, blocks the CPU: loading only); nothing else touches the upload pool.
- Render views: create after `Renderer::Init`, destroy in `OnDetach`; resize with `SetSize`; show with `ImGui::Image(view.GetImGuiTexture(), ...)`. Window resize rebuilds only the swapchain. `EditorCamera` options are `EditorCameraSettings`.
- Pipeline rebuilds go through `Renderer2D::RebuildQuadPipeline` (build new first, keep old on failure, swap; the old one goes through the deletion queue).
- **Textures**: `Texture2D` from `ImageSpecification` + `TextureProperties` or `Texture2D::LoadFromFile` (shared by file, `nullptr` on failure); draw with `Renderer2D::DrawQuad(transform, texture, tiling, tint)`. 32 slots per draw (slot 0 = white); `Renderer2D` starts a new draw when they run out; the deletion queue keeps a dropped texture's image alive until the frames in flight are done. The shader's `inTextures[32]` and `switch` must match `MaxTextureSlots`.
- **Assets (TODO)**: until the asset system (AS1-AS5, steps in `docs/Architecture.md#planned-asset-system`), components hold `Ref<Texture2D>` and scenes save the path relative to the asset folder. Mark such code `TODO(assets)`; don't add another way to reference assets.

## Editor (Oxygen)

- Dockable windows are `<Name>Window` classes in `Oxygen/src/EditorWindows` with `OnImguiRender(...)` and `bool& IsOpen()`, listed in the View menu; sub-sections are `<Name>Panel`. Windows don't save files: they expose callbacks (`OnSaveProject`, `OnOpenScene`) that `EditorLayer` sets. Edits and saves go to the editor scene; windows show the active scene.
- Inspector values are typed fields (`InputFloatField`, `DrawVec3Control`), not drag bars, clamped when the field is left.
- The look is Oxygen-only (`EditorStyle`, `EditorSettings`, `EditorChrome`); no hard-coded UI colours (`ImGui::GetStyle().Colors`, `EditorStyle::GetHighlightColor()`, `PlayModeSettings::TintColor`), readable on the light theme; decorations a theme can't express go in `EditorChrome`.
- A new preference: field with a default in `EditorSettings`, read/written in `EditorSettingsSerializer` (default on missing key), a row in `EditorSettingsWindow`; bump `Version` only when old values must be ignored; the session is saved with `SaveSession`, never a full `Save`.
- Fonts come from `ApplicationSpecification::ImGui`; `ImGuiLayer::GetFont("Bold")` may return `nullptr`; font size needs a restart. Paths come from `EditorPaths`; table helpers from `StatsUI`.

## Code conventions

- Everything in `namespace Hydro`: no other, nested or anonymous namespaces; file-local helpers are `static`; related helpers are a class with static functions. Exceptions: `std::hash<>`, `YAML::convert<>`.
- No `!` operator anywhere: `== false`, `== nullptr`.
- Braces on every `if`/`else`/`for`/`while`/`do` body, each brace on its own line; only `case X: return Y;` may stay on one line. Add braces to old code you edit.
- `Ref`/`Scope` with `CreateRef`/`CreateScope`. Members `m_PascalCase`, statics `s_PascalCase`, PascalCase types, methods and spec fields; `enum class Name : type`, never `typedef enum class`. Don't copy known misspellings (`HierachyWindow`, `heigth`, `GetPipeLine`).
- One-argument constructors `explicit`; `const Ref<T>&` for refs (never non-const `Ref<T>&`), `T&` when you only use the object; `const` getter overloads; `[[maybe_unused]]` instead of deleting parameter names. Don't bind temporaries to non-const references (MSVC accepts it, other compilers don't).
- `VK_CHECK_RESULT(...)` on `VkResult` calls; `HY_CORE_ASSERT(condition, message)`.
- Includes: forward slashes, engine headers from `src` (`"Hydro/Scene/Entity.h"`, never `../`; same-folder bare names are fine), vendored libraries through their include dir (`<entt.hpp>`, `<stb_image.h>`, `<imgui.h>`), never `vendor/` paths. A new vendored library gets an `IncludeDir` in `Dependencies.lua` and goes in `ExternalIncludeDirs { ... }` in the project's `premake5.lua`. `#pragma once`; include what you use; never include `hypch.h` in headers.
- Format with `.clang-format`; follow `.editorconfig` (CRLF, tabs, UTF-8; 2 spaces for YAML files). `/W4`, no new warnings. Don't reformat files you aren't otherwise changing.

## Docs and wiki

- Edit `docs/`, then `python scripts/regen_wiki.py --wiki <Hydro.wiki>` (`--check` to verify) for the mirrored pages (Architecture, Resource-Creation, Scene-and-ECS, Subsystems). Update `docs/CodeMap.md` when files are added, renamed or deleted.
- Wiki-only pages by hand: Components (component changes), Known-Issues (remove fixed rows, add them under "Recently fixed"), Roadmap (status, tick task boxes), Coding-Conventions. Close or comment on the related GitHub issue when you fix something.
- `.qwen/skills/hydro-engine-architecture/` holds a copy of these rules for a local model; when a rule here changes, update it too (`.qwen/INSTALL.md`).

## Before finishing

- [ ] Everything in `namespace Hydro`; file-local helpers `static`
- [ ] New engine services are subsystems: named by the table, `Init`/`Shutdown` only from `Application` at mirrored positions, no work in constructor/destructor, double-init assert; parts started/stopped by their owner in reverse
- [ ] Layers release in `OnDetach` what they created in `OnAttach`
- [ ] Timing from `Time`; new timed work has a `FramePhase`
- [ ] Live options are `<Name>Settings` with `Get`/`Set`; anything to keep is in `ProjectConfig`, serialized with defaults for missing keys
- [ ] New resources take a `Specification`/`Properties`, own their handles, are non-copyable, held in `Scope<T>`, and free through `Renderer::SubmitResourceFree` (no `Destroy()`, no `Ref<Vulkan...>` members, no `WaitIdle` to release)
- [ ] No Vulkan types in engine-level specs or new `Renderer/` headers; the engine loads nothing from `assets/`
- [ ] New behaviour is a system over a view (or a script if truly per-entity), in a deliberate place in the frame order
- [ ] Components are plain data; new ones are saved, in `CopyableComponents` and in the inspector
- [ ] Game code follows the Scripting plan's layers and folders: content in data, run state on the session (not statics), pure rules, views that only present, references instead of tag lookups
- [ ] Projections from `Camera::Perspective` / `Orthographic`
- [ ] GPU work in the frame's command buffer; per-frame resources per frame in flight
- [ ] No `!`; braces everywhere; forward-slash includes from `src`; `#pragma once`
- [ ] Naming rules; no new warnings at `/W4`; formatted; CRLF kept
- [ ] `scripts/check.sh` on the changed files; the summary says an MSVC build is still needed
- [ ] `docs/` updated and the wiki regenerated if the change adds a subsystem, setting, spec, component, system or pass, or changes ownership, startup/shutdown or frame order; `CodeMap.md` updated for new/renamed/deleted files; Components / Known-Issues / Roadmap updated; fixed Known gaps removed

In the summary, say which rule each part of the change follows. If something had to break a rule, list it as a known gap in the docs instead of hiding it.