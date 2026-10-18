# Hydro architecture

Hydro is a Vulkan game engine built as a static library (`Hydro`) that applications link against. The editor, **Oxygen**, is one of those applications; a game runtime would be another.

Read this first, then:

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
| `Renderer` | Engine-facing rendering API (`Renderer`, `RendererSettings`, `Renderer2D`, `RenderView`, `SceneRenderer`, `Camera`, `SceneCamera`, `EditorCamera`, `Image.h`) | Core, Platform/Vulkan |
| `Scene` | Scene, entities, components, scripts and `ScriptRegistry`, scene copy for play mode, scene and prefab saving (yaml-cpp) | Core, Utils, Renderer (submission only) |
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
   1. wait for this frame-in-flight's fence, acquire a swapchain image;
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
Renderer2D::EndScene();
```

- Each view begun in a frame gets its own pass in `Renderer2D::Render`, in `BeginScene` order. A view not begun in a frame isn't rendered (Oxygen skips hidden windows).
- `view.SetSize(w, h)` resizes it from the next render (the editor follows its window sizes).
- The quad pipeline is built once, against a 1×1 compatibility render pass and a layout-only descriptor set. Every view's target has the same format and its set 0 (camera + textures) is built the same way, so `VulkanPipeline::BindDescriptorSet` binds the view's own set.
- Views are resources: the application creates them after `Renderer::Init` and destroys them while the renderer is alive (Oxygen does it in `OnDetach`).

Scenes are drawn through a `CameraComponent` at runtime or the `EditorCamera` in the editor; see [Scene, ECS and systems](SceneAndECS.md#cameras).

## The editor (Oxygen)

`EditorLayer` owns the editor scene, the play copy, two `RenderView`s, the `EditorCamera` and the editor windows. Windows are `<Name>Window` classes in `Oxygen/src/EditorWindows`, listed under **View**.

| Window | What it does |
| --- | --- |
| **Editor** | The active scene through the editor camera (Camera menu: 2D/orthographic, reset). Mouse input reaches the camera only while it is hovered |
| **Game** | The active scene through its primary `CameraComponent`; its size is the scene's viewport |
| Top bar | **Play / Stop**, **Pause / Resume**, **Step** (see [Play mode](SceneAndECS.md#play-mode)) |
| **Hierarchy** | Entities with a Tag and ID; create, delete, save as prefab, drop prefabs |
| **Inspector** | The selected entity's components, edited with typed input fields; Add Component (including registered scripts) |
| **Project** | The project's asset folder; open scenes, drag prefabs into the scene |
| **Console** | The `Log` history (engine and client), with level toggles, a text filter and auto-scroll |
| **Stats**, **Render Settings**, **Project Settings** | Frame timings and renderer stats; live `RendererSettings`; project name and start scene |

Only visible windows are rendered: a hidden Editor or Game tab doesn't begin its view that frame.

## Vulkan ownership

| Object | Owns |
| --- | --- |
| `VulkanRendererContext` | Instance, debug messenger (Debug builds), window surface, device |
| `VulkanDevice` | Logical device, graphics + present queues, upload command pool |
| `VulkanSwapChain` | Swapchain images, views, framebuffers, present render pass, render-finished semaphores; acquire/present; `Recreate` |
| `VulkanFrameContext` | Per frame in flight (`MAX_FRAMES_IN_FLIGHT = 2`): command pool + buffer, fence, image-acquired semaphore |
| `Renderer` | The above, and runs the frame |
| `VulkanRenderPass` | An offscreen colour target: one image + framebuffer per frame in flight. Each `RenderView` owns one |
| `RenderView` | Its target, a uniform buffer + descriptor set for its camera, and its quad batches (per-frame vertex buffers) |

Rules that follow from this:

- **Anything the CPU writes per frame exists once per frame in flight** (vertex buffers, uniform buffers, offscreen images). Index it with `Renderer::GetRenderFrame()`.
- **Record into the frame's command buffer** (`Renderer::GetCurrentCommandBuffer()` or the one passed to you). Don't create a separate submit for per-frame work.
- **Window resize rebuilds only the swapchain.** Render views follow the windows that show them (`RenderView::SetSize`; the Game window also calls `Scene::OnViewportResize` for camera aspect ratios).
- **One-off uploads** (textures, static buffers) use `VulkanCommandBuffer`, which takes the device's upload pool and waits for completion.

## Shaders

Shaders live as GLSL in the application's `assets/shaders` folder and are referenced by their source path (`assets/shaders/vertex.vert`). `VulkanShader` compiles them with shaderc when it is created, so there is no separate compile step to run.

- **Build-time check.** Oxygen's project also runs `glslc` on every shader as a Visual Studio build step, so syntax errors show up in the Error List. The `.spv` files it writes are not used at runtime yet; they are there for shipping precompiled shaders later.
- **Hot reload.** `RendererSettings::ShaderHotReload` (on by default, off in Dist; toggle it in **Render Settings**). `Renderer2D` watches its shader's sources and `#include`d files with a `FileWatcher` (polled every 250 ms). On a change it compiles a new `VulkanShader` and `VulkanPipeline` from the old specifications, waits for the GPU (`Renderer::WaitIdle`), then swaps them in. If compilation fails, the error is logged and the old shader keeps drawing.
- **Runtime dependency.** shaderc is a DLL: Oxygen's post-build step copies `shaderc_shared.dll` from `$(VULKAN_SDK)\Bin` next to the executable. Debug builds link the release shaderc library on purpose (it is only used through its C API).

## Code conventions

- `Ref<T>` (shared) and `Scope<T>` (unique) from `Core/Base.h`; create with `CreateRef` / `CreateScope`.
- Members `m_Name`, statics `s_Name`, types and methods `PascalCase`.
- No logical-not operator: write `== false` for booleans and `== nullptr` for pointers.
- Check Vulkan results with `VK_CHECK_RESULT(...)`.
- Subsystems and their parts have `Init` / `Shutdown` (never `Create` / `ShutDown` / `Destroy`); hooks the engine calls on client code start with `On`. See [Subsystems](Subsystems.md).
- Debug checks use `HY_CORE_ASSERT(condition, message)` from `Core/Assert.h`.
- Includes use forward slashes (`#include "Hydro/Core/Base.h"`), and every header has `#pragma once`.

## Known gaps

These are where the code does not yet match the rules above. Fix them when you touch the area rather than copying the pattern. Every open problem is a [GitHub issue](https://github.com/joelnl74/Hydro/issues).

- `Renderer2D` includes `imgui_impl_vulkan.h` to hand the viewport image to ImGui.
- Each `RenderView` has its own quads, so a scene shown in two windows is submitted (and its vertices uploaded) twice.
- A `RenderView`'s ImGui textures stay in ImGui's descriptor pool until ImGui shuts down; the vendored ImGui has no `ImGui_ImplVulkan_RemoveTexture`.
- `Scene::m_Registry` is public; the editor reads it directly ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- Most Vulkan wrappers still have a manual `Destroy()` instead of a destructor (see [Resource creation](ResourceCreation.md#ownership)).
- Shader hot reload rebuilds the pipeline from its existing `PipelineSpecification`, so a shader whose descriptor bindings or vertex inputs change still needs `Renderer2D`'s setup changed and a restart. Only `Renderer2D`'s quad shader is watched; there is no shader library yet.
- `VkInstance` and the device are `static` members of `VulkanRendererContext` ([#50](https://github.com/joelnl74/Hydro/issues/50)).
- `VulkanDevice` and `VulkanSwapChain` still do their setup in the constructor (+ `Create`) instead of `Init` (see [Subsystems](Subsystems.md#known-gaps)).
