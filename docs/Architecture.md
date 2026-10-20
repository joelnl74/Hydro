# Hydro architecture

Hydro is a Vulkan game engine built as a static library (`Hydro`) that applications link against. The editor, **Oxygen**, is one of those applications; a game runtime would be another.

Read this first, then:

- [Code map](CodeMap.md) — every source file in one line, and which files each kind of change touches. Start here to find things.
- [Resource creation](ResourceCreation.md) — how GPU and engine objects are described and created (Parameter Object pattern).
- [Scene, ECS and systems](SceneAndECS.md) — how scenes, entities, components and systems fit together.
- [Subsystems](Subsystems.md) — startup and shutdown order, the lifecycle rules, and how engine services are named.

## Projects

| Project | Kind | What it is |
| --- | --- | --- |
| `Hydro` | Static library | The engine: core, events, platform, renderer, scene, project |
| `Oxygen` | Executable | The editor. An `Application` subclass that registers its example scripts and pushes an `EditorLayer` (see [The editor](#the-editor-oxygen)) |

An application defines `Hydro::CreateApplication()`, includes `Hydro/Core/EntryPoint.h` once, and adds its features as `Layer`s:

```cpp
class MyApp : public Hydro::Application
{
public:
    MyApp(const Hydro::ApplicationSpecification& spec) : Application(spec) {}
    void OnInit() override { PushLayer(new MyGameLayer()); }
};

Hydro::Application* Hydro::CreateApplication()
{
    Hydro::ApplicationSpecification spec;
    spec.Name = "My Game";
    spec.MainWindow.Title = "My Game";
    return new MyApp(spec);
}
```

## Startup and shutdown

Everything is started and stopped explicitly; constructors and destructors do no engine work. `main` calls `Init` → `Run` → `Shutdown` on the application, and the application starts its **subsystems** in a fixed order and stops them in exactly the reverse order:

| Init | Shutdown |
| --- | --- |
| 1. `Log` | 7. client `OnShutdown` |
| 2. `Time` | 6. wait for the GPU, detach layers (ImGui included) |
| 3. `EventDispatcher` | 5. `Renderer` |
| 4. `Window` | 4. `Window` |
| 5. `Renderer` (context, swapchain, frames, allocator, `Renderer2D`, `Renderer3D`) | 3. `EventDispatcher` |
| 6. `ImGuiLayer` overlay | 2. `Time` |
| 7. client `OnInit` (pushes layers) | 1. `Log` |

The rules, the vocabulary (subsystem, part, layer, system, resource) and how to name new subsystems are in [Subsystems](Subsystems.md).

## Source layout (`Hydro/src/Hydro`)

| Folder | Responsibility | May depend on |
| --- | --- | --- |
| `Core` | Application, `LayerStack` and layers, window interface, logging (`Log`, which also keeps the last 5000 messages in memory for `Log::GetMessagesSince`; Oxygen's **Console** window shows them), frame clock (`Time`), asserts, input, UUID, `FileWatcher` | — |
| `Events` | Event types and the dispatcher | Core |
| `Platform/Windows` | GLFW window, input, file dialogs | Core, Events |
| `Platform/Vulkan` | Everything that talks to Vulkan | Core |
| `Renderer` | Engine-facing rendering API (`Renderer`, `RendererSettings`, `Renderer2D`, `RenderView`, `Texture2D`, `SceneRenderer`, `Camera`, `SceneCamera`, `EditorCamera`, `Image.h`) | Core, Platform/Vulkan |
| `Scene` | Scene, entities, components, scripts and `ScriptRegistry`, scene copy for play mode, scene and prefab saving (yaml-cpp) | Core, Utils, Renderer (submission, and `Texture2D` for sprites). Until the asset system, `EntitySerializer` also loads sprite textures and reads `Project` for the asset folder (see [Planned: asset system](#planned-asset-system)) |
| `Project` | Project file (`.hyproj`, YAML) and settings | Core, Utils, Renderer (`RendererSettings` only) |
| `Utils` | Platform file dialogs, shared YAML conversions for glm types | Core |
| `ImGUI` | ImGui layer | Core, Renderer |

Applications include `Hydro.h` and should not include `Platform/Vulkan` headers directly.

## Projects and settings

A project is a folder with a `.hyproj` file (YAML, like scenes) and an asset folder. `ProjectSerializer` reads and writes it; keys missing from the file keep their defaults, so old projects load after settings are added.

```yaml
Project:
  Name: Sandbox
  AssetDirectory: Assets              # relative to the .hyproj folder
  StartScene: Scenes/Scene.hyscene    # relative to AssetDirectory; empty = none
Renderer:
  ClearColor: [0, 0, 0, 1]
  VSync: true
  Wireframe: false
  ShaderHotReload: true
```

- `Project::Load(file)` / `Project::New(file)` make a project active; `Project::SaveActive()` writes it; `Project::Close()` releases it. `Project` doesn't call other subsystems: whoever opens a project applies its settings (`Renderer::SetSettings(config.Renderer)`) and, when saving, copies the live settings back (`config.Renderer = Renderer::GetSettings()`).
- `RendererSettings` are live: `Renderer::SetSettings` applies them from the next frame. Clear colour and hot reload are read every frame, wireframe rebuilds the 2D pipeline, and vsync recreates the swapchain. `Renderer::GetCapabilities()` says what the GPU supports (wireframe, wide lines).
- Oxygen opens the first `.hyproj` in its `project` folder at startup (`Oxygen/project/Sandbox.hyproj`). **View > Render Settings** edits the renderer settings live and **View > Project Settings** edits the name and start scene; **File > Save Project** (or the Save buttons) writes both.

## The frame

`Application::Run` does the same steps every frame, each timed by the `Time` subsystem as a `FramePhase`:

1. **Events**: `Window::OnUpdate` polls GLFW; events go to the application, then to layers from the top down.
2. **UI**: `ImGuiLayer::Begin`, every layer's `OnImGuiRender`, then `ImGuiLayer::End`.
3. **Update**: every layer's `OnUpdate(Time::GetDeltaTime())`. A layer updates its scene here (`Scene::OnUpdateRuntime` or `OnUpdateEditor`, then its own systems) and submits draw calls into a `RenderView` through a camera (`SceneRenderer::RenderRuntime` or `RenderEditor` → `Renderer2D::BeginScene(view, ...)` + `DrawQuad` + `EndScene`). Nothing touches the GPU yet.
4. **Render**: `Renderer::RenderFrame` records and submits the whole frame in **one** command buffer:
   1. wait for this frame-in-flight's fence, run the deletion queue's frees whose frames are now finished (`Renderer::SubmitResourceFree`, see [Vulkan ownership](#vulkan-ownership)), acquire a swapchain image;
   2. `Renderer2D::Render` — one offscreen 2D pass per `RenderView` begun this frame (its batched quads, seen through its camera), each cleared to `RendererSettings::ClearColor`. It applies each view's pending resize before its pass, and first rebuilds its pipeline if `Wireframe` changed and, when hot reload is on, rebuilds its shader and pipeline if a shader source changed (see [Shaders](#shaders));
   3. the swapchain pass — ImGui draws the editor UI, including the Editor and Game windows, which sample their views' images;
   4. submit, present.

   The renderer reports steps 1–3 to `Time` as the `Render` phase and step 4 as `Present`.
5. **Platform windows**: ImGui windows dragged outside the main window are drawn.

While the window is minimised, step 4 is skipped and the queued quads are discarded.

## Cameras and clip space

Every projection matrix is built by `Camera::Perspective` / `Camera::Orthographic` (`Renderer/Camera.h`), so Vulkan's clip-space conventions are handled in one place: depth is `[0, 1]` (GLM's `*_ZO` functions; no `GLM_FORCE_DEPTH_ZERO_TO_ONE` define) and Y is flipped, so world +Y is up on screen. Because of the flip, geometry is counter-clockwise (`VulkanPipeline` uses `VK_FRONT_FACE_COUNTER_CLOCKWISE`) and the offscreen image is shown with ordinary UVs.

## Render views

A `RenderView` (`Renderer/RenderView.h`, created from a `RenderViewSpecification`) is one picture of a scene: an offscreen image (a `VulkanRenderPass`), the camera it is seen through, and the quads queued for it this frame. Oxygen has two, shown in its **Editor** and **Game** windows with `ImGui::Image(view.GetImGuiTexture(), ...)`.

```cpp
Renderer2D::BeginScene(view, camera, cameraTransform); // or BeginScene(view, viewProjection)
Renderer2D::DrawQuad(transform, color);
Renderer2D::DrawQuad(transform, texture, tilingFactor, tintColor); // Ref<Texture2D>; null = white
Renderer2D::EndScene();
```

- Each view begun in a frame gets its own pass in `Renderer2D::Render`, in `BeginScene` order. A view not begun in a frame isn't rendered (Oxygen skips hidden windows).
- `view.SetSize(w, h)` resizes it from the next render (the editor follows its window sizes). The old images go through the deletion queue, so a resize doesn't wait for the GPU; each frame index's ImGui texture handle is pointed at its new image when that index is recorded next.
- The quad pipeline is built once, against a 1×1 compatibility render pass and a layout-only descriptor set. Every view's target has the same format and its set 0 (camera + textures) is built the same way, so `VulkanPipeline::BindDescriptorSet` binds the view's own set.
- Views are resources: the application creates them after `Renderer::Init` and destroys them while the renderer is alive (Oxygen does it in `OnDetach`).

### Textures and batching

`Texture2D` (`Renderer/Texture2D.h`) is the engine-facing texture: RGBA8, made from an `ImageSpecification` (size, `ImageFormat::SRGB` for colour or `RGBA` for data) and `TextureProperties` (filter, wrap), on a `VulkanImage` (VMA, one upload). `Texture2D::LoadFromFile(path)` reads PNG/JPG/TGA/BMP with stb_image and returns `nullptr` (logged) on failure; while a texture from the same file is alive it is shared instead of loaded again. Textures are shared by many sprites, so they are held in `Ref<Texture2D>`. The header has no Vulkan types: the image lives in `Texture2DData` (`Platform/Vulkan/VulkanTexture2DData.h`).

A view's quads are recorded as a list of **draws** (one `vkCmdDrawIndexed` each), in submission order:

- **Batches** hold vertices: up to 20,000 quads per vertex buffer (one per frame in flight). When one is full the next is used, allocated the first time a frame needs it.
- **Texture sets** hold up to 32 textures (`Renderer2D::GetMaxTextureSlots()`; slot 0 is a 1×1 white texture made in code, so untextured quads need no file) and set 0's descriptor sets (camera + texture array, one per frame in flight). `DrawQuad` looks the texture up in the current set and adds it to a free slot.
- A new draw starts when the batch is full or when a quad needs a 33rd texture (a **texture flush**: the next set is used, with fresh slots). A flush costs one more draw call, not a new vertex buffer, and earlier draws are recorded first, so draw order is kept. There is no limit on quads or textures per frame. Batches and sets are reused every frame; nothing is allocated once a scene has been drawn.
- `RecordView` writes each used set's texture array into that frame's descriptor set before the pass (the frame's fence has signalled, so the GPU is done with it), uploads the batches, then binds vertex buffer and descriptor set only when they change between draws.
- **Lifetime.** A texture set holds `Ref`s to its textures only while the frame is being built. When the last `Ref` goes, the texture's `VulkanImage` hands its handles to the deletion queue, which frees them after the frames in flight that may sample it. So dropping a texture mid-frame (changing a sprite's texture, deleting an entity) is safe without `Renderer::WaitIdle`.
- The quad shader picks the texture with a `switch` over constant indices (`textureGrad` with derivatives taken before it), because indexing the sampler array with a per-quad value needs descriptor indexing, which the device doesn't enable. `texIndex` is `flat`. The vertex colour multiplies the texture, alpha included.
- Texture coordinates run `v = 1` at the quad's bottom to `v = 0` at its top (Vulkan's first image row is the top), times the tiling factor; samplers repeat by default.
- **Stats** (Oxygen's Stats window): slots per draw, slots used, texture flushes.

Scenes are drawn through a `CameraComponent` at runtime or the `EditorCamera` in the editor; see [Scene, ECS and systems](SceneAndECS.md#cameras).

## The editor (Oxygen)

`EditorLayer` owns the editor scene, the play copy, two `RenderView`s, the `EditorCamera` and the editor windows. Windows are `<Name>Window` classes in `Oxygen/src/EditorWindows`, listed under **View**.

| Window | What it does |
| --- | --- |
| **Editor** | The active scene through the editor camera (Camera menu: 2D/orthographic, reset). Mouse input reaches the camera only while it is hovered |
| **Game** | The active scene through its primary `CameraComponent`; its size is the scene's viewport |
| Top bar | The Hydro droplet, the menus and icon buttons for **Play / Stop**, **Pause / Resume**, **Step** (see [Play mode](SceneAndECS.md#play-mode)). A thin accent "waterline" runs under it |
| **Hierarchy** | Entities with a Tag and ID; create, delete, save as prefab, drop prefabs |
| **Inspector** | The selected entity's components, edited with typed input fields; Add Component (including registered scripts) |
| **Project** | The project's asset folder; open scenes, drag prefabs into the scene |
| **Console** | The `Log` history (engine and client), with level toggles, a text filter and auto-scroll; time and source columns follow Editor Settings |
| **Stats**, **Render Settings**, **Project Settings** | Frame timings and renderer stats; live `RendererSettings`; project name and start scene |
| **Editor Settings** | Pages: **Appearance** (theme, accent colour, text scale, font size), **Play Mode** (tint), **Layout** (rounding, borders, spacing), **Colors** (every ImGui colour), **Editor Camera** (start in 2D, field of view, clip planes, orbit/pan/zoom speed), **Console**, **Startup** (reopen last project and scene, autosave). Applied live; **Save** writes `EditorSettings.yaml` next to `imgui.ini` (per user, not per project); **Revert** reloads it; **Defaults** resets everything |

Only visible windows are rendered: a hidden Editor or Game tab doesn't begin its view that frame.

The editor's look is editor-only: `EditorStyle`, `EditorSettings` (with `EditorSettingsSerializer`) and `EditorChrome` live in Oxygen, and the engine never reads them. The engine's `ImGuiLayer` sets ImGui's dark defaults; `EditorLayer::OnAttach` loads `EditorSettings.yaml` and applies the saved style on top. Missing keys keep the theme's values, so the file still loads after a setting is added. The file has a `Version`; a file from an older version keeps its theme and font scale, and its colours and sizes are rebuilt from the theme.

- **Themes.** **Hydro** (deep ocean blues, the default), **Abyss** (darker) and **Glacier** (light) are built from one accent colour (presets Aqua, Lagoon, Tide, Orchid, Coral, Amber, or any colour): selection, checkmarks, sliders, tabs, focus and the waterline all follow it. **Dark**, **Light** and **Classic** are ImGui's presets.
- **Chrome.** `EditorChrome` draws what a theme can't: the droplet and name at the start of the menu bar, the waterline, the play-control icons and the play-mode tint. While playing, windows, title bars, tabs and the menu bar blend towards the tint colour, the waterline takes the tint colour and the Game window gets an outline.
- **Fonts.** Oxygen loads Open Sans (`Oxygen/assets/Fonts/opensans`, Regular and Bold) through `ApplicationSpecification::ImGui` (an `ImGuiLayerSpecification`). The engine loads only the fonts the application lists; `ImGuiLayer::GetFont("Bold")` finds one by name. The font atlas is built once at startup, so **Font size** applies after a restart (it is read in `CreateApplication`, before the log exists); **Scale** applies at once.
- **Editor camera.** `EditorCameraSettings` (engine, `Renderer/EditorCamera.h`) are live options passed in with `EditorCamera::SetSettings` every frame; changing the field of view keeps the framing at the focal point.
- **Session.** The last project and scene are remembered in the same file. On close, `EditorSettingsSerializer::SaveSession` rewrites only that part, so unsaved edits in the window aren't written by accident.

## Vulkan ownership

| Object | Owns |
| --- | --- |
| `VulkanRendererContext` | Instance, debug messenger (Debug builds), window surface, device (`Scope<VulkanDevice>`, handed out as `VulkanDevice&`) |
| `VulkanDevice` | The GPU it was created for (`Scope<VulkanPhysicalDevice>`), logical device, graphics + present queues, upload command pool |
| `VulkanSwapChain` | Swapchain images, views, framebuffers, present render pass, render-finished semaphores; acquire/present; `Recreate` |
| `VulkanFrameContext` | Per frame in flight (`MAX_FRAMES_IN_FLIGHT = 2`): command pool + buffer, fence, image-acquired semaphore |
| `Renderer` | The above (`Scope`s; `GetRendererContext()` and `GetVulkanSwapChain()` return references), the **deletion queue**, and runs the frame |
| `VulkanRenderPass` | An offscreen colour target: one image + framebuffer per frame in flight. Each `RenderView` owns one |
| `RenderView` | Its target, a uniform buffer for its camera, its quad batches (per-frame vertex buffers) and texture sets (per-frame descriptor sets: camera + 32 textures), all in `Scope`s |
| `Renderer2D` | The quad shader, pipeline, index buffer, the 1×1 compatibility render pass with its layout descriptor set, and the white texture |
| `Texture2D` | One `VulkanImage` (image, view, sampler; VMA). Shared through `Ref<Texture2D>` |

GPU resources (`VulkanImage`, `VulkanRenderPass`, `VulkanVertexBuffer`, `VulkanIndexBuffer`, `VulkanUniformBuffer`, `VulkanDescriptorBuilder`, `VulkanPipeline`, `VulkanShader`) free their handles in their destructor and are non-copyable; their single owner holds them in a `Scope<T>` (see [Resource creation](ResourceCreation.md#ownership)). A destructor doesn't destroy the handles itself: it hands a function that does to the **deletion queue**, `Renderer::SubmitResourceFree(std::function<void()>)`. The renderer tags it with the current frame number and runs it at the start of the frame `MAX_FRAMES_IN_FLIGHT` frames later, once that frame slot's fence says the GPU is done with every frame that could have used the resource. It also runs everything queued after a swapchain rebuild (the device is idle then) and at `Renderer::Shutdown`, before the allocator and device go. Frees run in the order they were queued. `VulkanShader` is the exception: shader modules aren't used after a pipeline is created, so it frees them at once.

Rules that follow from this:

- **Anything the CPU writes per frame exists once per frame in flight** (vertex buffers, uniform buffers, offscreen images). Index it with `Renderer::GetRenderFrame()`.
- **Record into the frame's command buffer** (`Renderer::GetCurrentCommandBuffer()` or the one passed to you). Don't create a separate submit for per-frame work.
- **Window resize rebuilds only the swapchain.** Render views follow the windows that show them (`RenderView::SetSize`; the Game window also calls `Scene::OnViewportResize` for camera aspect ratios).
- **One-off uploads** (textures, static buffers) use `VulkanCommandBuffer`, which takes the device's upload pool and waits for completion.
- **Never wait for the GPU to release something.** Drop the `Scope` (or the last `Ref`) and the deletion queue does the rest. `Renderer::WaitIdle()` is only for shutdown and swapchain rebuilds.

## Shaders

Shaders live as GLSL in the application's `assets/shaders` folder and are referenced by their source path (`assets/shaders/vertex.vert`). `VulkanShader` compiles them with shaderc when it is created, so there is no separate compile step to run. The quad shader's `sampler2D inTextures[32]` and its `switch` must match `Renderer2D`'s texture slot count (`VulkanDescriptorBuilder::MaxImageArraySize`, checked by a `static_assert`).

- **Build-time check.** Oxygen's project also runs `glslc` on every shader as a Visual Studio build step, so syntax errors show up in the Error List. The `.spv` files it writes are not used at runtime yet; they are there for shipping precompiled shaders later.
- **Hot reload.** `RendererSettings::ShaderHotReload` (on by default, off in Dist; toggle it in **Render Settings**). `Renderer2D` watches its shader's sources and `#include`d files with a `FileWatcher` (polled every 250 ms). On a change it compiles a new `VulkanShader` and `VulkanPipeline` from the old specifications and swaps them in; the old pipeline is freed through the deletion queue, so nothing waits for the GPU. If compilation fails, the error is logged and the old shader keeps drawing.
- **Runtime dependency.** shaderc is a DLL: Oxygen's post-build step copies `shaderc_shared.dll` from `$(VULKAN_SDK)\Bin` next to the executable. Debug builds link the release shaderc library on purpose (it is only used through its C API).

## Code conventions

- `Ref<T>` (shared) and `Scope<T>` (unique) from `Core/Base.h`; create with `CreateRef` / `CreateScope`.
- Members `m_Name`, statics `s_Name`, types and methods `PascalCase`.
- **Namespaces.** All engine and editor code lives in `namespace Hydro`: no other namespaces, no nested ones (`Hydro::Something`), and no anonymous `namespace { }`. File-local helpers are `static` functions (and `const`/`constexpr` variables) inside `Hydro`; a group of related helpers is a class with static functions (`EditorPaths`, `AssetDragDrop`, `StatsUI`, like `Input` or `Renderer2D`). The only exceptions are specializations a library requires in its own namespace: `std::hash<>` (`namespace std`) and `YAML::convert<>` (`namespace YAML`).
- No logical-not operator: write `== false` for booleans and `== nullptr` for pointers.
- **Braces, always.** Every `if`, `else`, `for`, `while` and `do` body is in `{ }`, even when it is one line (`else if` chains too). Braces go on their own lines (Allman). Only `case X: return Y;` lines in a `switch` may stay on one line. `.clang-format` adds missing braces (`InsertBraces`); older code without them is fixed when you touch it.

  ```cpp
  if (pixels == nullptr)
  {
      return false;
  }

  for (auto entity : view)
  {
      Update(entity);
  }
  ```
- Check Vulkan results with `VK_CHECK_RESULT(...)`.
- Subsystems and their parts have `Init` / `Shutdown` (never `Create` / `ShutDown` / `Destroy`); hooks the engine calls on client code start with `On`. See [Subsystems](Subsystems.md).
- Debug checks use `HY_CORE_ASSERT(condition, message)` from `Core/Assert.h`.
- Includes use forward slashes and every header has `#pragma once`. Engine headers are included from `src` (`#include "Hydro/Core/Base.h"`), never with `../` (`"../Scene/Entity.h"`); a header next to the file may use its bare name (`"VulkanUtils.h"`). Vendored libraries are included through their include directory (`<entt.hpp>`, `<stb_image.h>`, `<tiny_obj_loader.h>`), never by a path into `vendor/`.
- **Naming.** Members are `m_PascalCase` (`m_RenderPass`, not `m_renderPass`); fields of specs and other plain structs are `PascalCase` (`PipelineSpecification::Shader`; `ShaderInformation::path` is still being renamed). Enums are `enum class Name : type`, never `typedef enum class`. Spell names right (`VullkanTexture`, `HierachyWindow`, `heigth` and `GetPipeLine` are being renamed).
- **Signatures.** Take shared pointers you don't keep as `const Ref<T>&`; take objects you only use as `T&` / `const T&`. Constructors with one argument are `explicit`. Getters have `const` overloads (`Entity::GetComponent`, `HasComponent`).
- **Ownership.** An object that creates a GPU resource and is its only owner holds it in a `Scope<T>`; `Ref<T>` is only for objects that really are shared. See [Resource creation](ResourceCreation.md#ownership).
- Mark parameters a function must accept but doesn't use `[[maybe_unused]]` (`virtual void OnUpdate([[maybe_unused]] float deltaTime) {}`) instead of removing their names.

## Tooling

The repository root has the files that keep code consistent; Visual Studio reads all of them.

| File | What it does |
| --- | --- |
| `.clang-format` | Tabs (4 wide), Allman braces, indented `namespace Hydro`, `public:` at class level, `Type* name`, no column limit (lines aren't rewrapped), sorted includes. Format the file you're editing with Ctrl+K, Ctrl+D; don't reformat the whole repository in a commit that also changes behaviour. Needs clang-format 17 or newer (Visual Studio 2022 17.8+). `Hydro/vendor` and `vendor` have a `.clang-format` that turns formatting off |
| `.editorconfig` | UTF-8, CRLF, tabs (4), final newline, no trailing whitespace. YAML and the files Hydro writes as YAML (`.hyscene`, `.hyprefab`, `.hyproj`) use 2 spaces. Vendored code is left alone |
| `.gitattributes` | Text files are stored as LF and checked out as CRLF; images, fonts and `.spv` are binary. After adding it once: `git add --renormalize .` |
| Compiler warnings | `Hydro` and `Oxygen` build at `/W4` (premake `warnings "Extra"`). Vendored headers are listed with `ExternalIncludeDirs { ... }` (`Dependencies.lua`): on the include path, and marked external with warnings off (`externalincludedirs` on premake 5.0.0-beta2+, `/external:I` + `/external:W0` on the bundled beta1). Vendored sources (including the `imgui_impl_*` copies in `Platform/Vulkan`) compile with warnings off. `FatalWarnings` in the root `premake5.lua` turns on `/WX`: set it to `true` once a Debug build has no warnings |

Planned: a test project and a GitHub Actions build (Roadmap A2).

## Agreed changes

Agreed on October 19, 2026 after reviewing the engine and editor code against these rules. Each is a task list on the [Roadmap](https://github.com/joelnl74/Hydro/wiki/Roadmap#architecture-and-code-quality); suggested order: tooling and CI (A1, A2), then the deletion queue and RAII (A3, A4), then `SystemContext` (A5) before physics starts.

| ID | Change | Details |
| --- | --- | --- |
| A1 | Tooling: `.clang-format`, `.editorconfig`, `.gitattributes`, `/W4`, warnings as errors once clean | [Tooling](#tooling). Partly done |
| A2 | Tests and CI: a doctest project and a GitHub Actions build | Tests for `UUID`, scene and prefab round trips, `Scene::Copy`, `ScriptRegistry`, `EditorSettings` load; `windows-latest` with the Vulkan SDK, Debug build, run the tests |
| A3 | RAII for the six wrappers that still have `Destroy()`, and a per-frame deletion queue | **Done (Oct 20, 2026).** `VulkanVertexBuffer`, `VulkanIndexBuffer`, `VulkanUniformBuffer`, `VulkanDescriptorBuilder` and `VulkanPipeline` free themselves in their destructor; `VullkanTexture` was deleted (textures are `Texture2D` on `VulkanImage`, VMA). `Renderer::SubmitResourceFree` frees after the frames in flight, so hot reload and resizes no longer call `Renderer::WaitIdle`. See [Vulkan ownership](#vulkan-ownership) |
| A4 | `Scope` instead of `Ref` for owned GPU objects | **Done (Oct 20, 2026).** No `Ref<Vulkan...>` members are left: owners hold `Scope<T>`, users get `T&` (`Renderer::GetRendererContext()`, `GetVulkanDevice()`, `GetPhysicalDevice()`, `GetVulkanSwapChain()` return references); `PipelineSpecification` points at its shader and descriptor sets without owning them |
| A5 | `SystemContext` and an ordered list of stages in `Scene` | Finishes E5. See [Scene, ECS and systems](SceneAndECS.md#planned-system-context-and-stages) |
| A6 | One component registry | Name, save/load and inspector drawing per component, so saving, the play copy and the inspector loop over one list (E4). See [Scene, ECS and systems](SceneAndECS.md#planned-component-registry) |
| A7 | No Vulkan in `Renderer/` headers | `Renderer.h`, `Renderer2D.h` and `RenderView.h` include `vulkan.h`. `RenderView` returns an opaque `ImTextureID` |
| A8 | No asset paths in the engine | Partly (Oct 20, 2026): the white texture is made in code. `Renderer2D` still loads `assets/shaders/vertex.vert` and `fragment.frag` from the application; engine shaders come from an engine resources folder or a shader library. Needed by the Runtime (RT1) |
| A9 | Smaller fixes | `LayerStack` owns layers as `Scope<Layer>` (no raw `new`/`delete`); `VK_CHECK_RESULT` asserts in Debug ([#35](https://github.com/joelnl74/Hydro/issues/35)); typed event dispatch and `Handled` ([#38](https://github.com/joelnl74/Hydro/issues/38)); `Entity` asserts (E6) |
| A10 | Naming | `m_PascalCase` members (`m_uniformBuffers`, `m_format`, `m_renderPass`, `m_commandBuffer`, `m_vmaAllocation`, `m_memory`, `m_layoutBinding`, `m_poolSizes`); PascalCase spec fields (`ShaderInformation::path`; drop the duplicate `shaderStage`/`shaderType`; `PipelineSpecification::Shader` is done); `HierachyWindow`, `heigth`, `GetPipeLine` (`VullkanTexture` is gone); `enum class` instead of `typedef enum class` (`KeyCodes.h`, `Input.h`) |
| A11 | Const and ownership in signatures | `const Ref<T>&` parameters (`VulkanDescriptorBuilder::BindBuffer` takes `const VulkanUniformBuffer&` since Oct 20, 2026), `explicit` single-argument constructors, `const` overloads of `Entity::GetComponent`/`HasComponent`, `"Hydro/..."` include paths instead of `"../Scene/..."` |

## Planned: asset system

**TODO (Roadmap AS1–AS5).** Today a sprite holds a `Ref<Texture2D>` and the scene file stores the texture's path (relative to the project's asset folder). That is deliberately small: an `AssetHandle` is only useful together with a registry that maps handles to files and survives renames, so the handle, the registry and the manager are one piece of work. Steps, in order:

1. **`AssetHandle` and `Asset`.** `using AssetHandle = UUID;` and `enum class AssetType : uint16_t { None, Texture2D, Scene, Prefab, ... }` in `Asset/Asset.h`; `Texture2D` (and later fonts, audio clips) derive from a small `Asset` base with `GetType()`. No Vulkan, no file paths in the type.
2. **Asset registry.** One file per project (`Assets/AssetRegistry.hyr`, YAML): handle → path relative to the asset folder, type, import settings (for textures: `TextureProperties` and sRGB). Rebuilt by scanning the asset folder at project open; new files get a new handle, known files keep theirs.
3. **`AssetManager` subsystem.** A registry-style subsystem (`...Manager`, see [Subsystems](Subsystems.md#current-and-planned-subsystems)) started after `Renderer`, at the planned position: `Init`/`Shutdown`, `OnProjectOpened(project)` (the application calls it, like `Renderer::SetSettings`), `GetAsset<T>(handle)` returning `Ref<T>` (loaded on first use, cached by handle), `ImportAsset(path)` → handle, `IsAssetHandleValid`. It owns what `Texture2D::LoadFromFile`'s sharing does today; per-type importers (texture via stb_image) live next to it.
4. **Components store handles.** `SpriteRendererComponent::Texture` becomes `AssetHandle Texture = 0;`; `SceneRenderer` resolves it with `AssetManager::GetAsset<Texture2D>` (null → white). `EntitySerializer` saves the handle (a `uint64_t`) and keeps reading the old `Texture: <path>` key, importing the file to get a handle, so existing scenes load. `EntitySerializer` stops including `Project.h` and stops loading files.
5. **Editor.** The Project window shows registered assets with thumbnails (an ImGui texture per `Texture2D`) and drags handles instead of paths; the sprite's texture field accepts them; the inspector shows a selected texture's import settings and re-imports on change.
6. **Hot reload.** A `FileWatcher` per loaded file; on change re-import into the same `Ref`/handle, freeing the old image through the deletion queue (A3), so sprites keep their handle.
7. **Later.** Async loading on the `JobScheduler` (C3) with a placeholder texture; an asset pack for `Dist` builds (RT7).

Until then, new code that references a texture holds a `Ref<Texture2D>` and is marked `TODO(assets)`, so the switch is easy to find.

## Known gaps

These are where the code does not yet match the rules above. Fix them when you touch the area rather than copying the pattern. Every open problem is a [GitHub issue](https://github.com/joelnl74/Hydro/issues).

- `Renderer2D` includes `imgui_impl_vulkan.h` to hand the viewport image to ImGui.
- Each `RenderView` has its own quads, so a scene shown in two windows is submitted (and its vertices uploaded) twice.
- A `RenderView`'s ImGui textures stay in ImGui's descriptor pool until ImGui shuts down; the vendored ImGui has no `ImGui_ImplVulkan_RemoveTexture`.
- `Scene::m_Registry` is public; the editor reads it directly ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- The deletion queue only knows the main frame's fence. ImGui windows dragged outside the main window are submitted separately by ImGui (`RenderPlatformWindowsDefault`), so a view image freed or rewritten after a resize could in theory still be sampled by such a window's previous submission. Not seen in practice; fixed properly when the ImGui upgrade lets those windows render through `Renderer`.
- `Renderer.h`, `Renderer2D.h` and `RenderView.h` include `vulkan.h` (A7).
- `Renderer2D` loads its shaders from the application's `assets/` folder (A8).
- Sprites reference textures directly (`Ref<Texture2D>`) and scenes save the file path; `EntitySerializer` loads textures and reads `Project` for the asset folder. A texture file that is missing when a scene loads is logged and the sprite draws untextured; saving the scene then drops the reference. Sampling options (`TextureProperties`) aren't saved per texture yet (always linear + repeat). All of this moves to the asset system ([Planned: asset system](#planned-asset-system)).
- The 32 texture slots are fixed (shader array size). `Renderer2D::Init` logs an error if the GPU allows fewer samplers per stage (Vulkan guarantees 16; desktop GPUs have far more).
- Textures load synchronously on the main thread (stb_image decode + one upload that waits for the GPU).
- `LayerStack` owns layers through raw pointers (A9); some members and spec fields don't follow the naming rules (A10).
- Shader hot reload rebuilds the pipeline from its existing `PipelineSpecification`, so a shader whose descriptor bindings or vertex inputs change still needs `Renderer2D`'s setup changed and a restart. Only `Renderer2D`'s quad shader is watched; there is no shader library yet.
- `VkInstance` and the device are `static` members of `VulkanRendererContext` ([#50](https://github.com/joelnl74/Hydro/issues/50)).
- `VulkanDevice` and `VulkanSwapChain` still do their setup in the constructor (+ `Create`) instead of `Init` (see [Subsystems](Subsystems.md#known-gaps)).
