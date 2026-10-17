# Hydro architecture

Hydro is a Vulkan game engine built as a static library (`Hydro`) that applications link against. The editor, **Oxygen**, is one of those applications; a game runtime would be another.

Read this first, then:

- [Resource creation](ResourceCreation.md) — how GPU and engine objects are described and created (Parameter Object pattern).
- [Scene, ECS and systems](SceneAndECS.md) — how scenes, entities, components and systems fit together.

## Projects

| Project | Kind | What it is |
| --- | --- | --- |
| `Hydro` | Static library | The engine: core, events, platform, renderer, scene, project |
| `Oxygen` | Executable | The editor. An `Application` subclass that pushes an `EditorLayer` |

An application defines `Hydro::CreateApplication()`, includes `Hydro/Core/EntryPoint.h` once, and adds its features as `Layer`s:

```cpp
class MyApp : public Hydro::Application
{
public:
    void OnInit() override { PushLayer(new MyGameLayer()); }
};

Hydro::Application* Hydro::CreateApplication() { return new MyApp(); }
```

## Source layout (`Hydro/src/Hydro`)

| Folder | Responsibility | May depend on |
| --- | --- | --- |
| `Core` | Application, layers, window interface, logging, input, UUID | — |
| `Events` | Event types and the dispatcher | Core |
| `Platform/Windows` | GLFW window, input, file dialogs | Core, Events |
| `Platform/Vulkan` | Everything that talks to Vulkan | Core |
| `Renderer` | Engine-facing rendering API (`Renderer`, `Renderer2D`, `SceneRenderer`, `Camera`, `Image.h`) | Core, Platform/Vulkan |
| `Scene` | Scene, entities, components, scripts, scene and prefab saving (yaml-cpp) | Core, Renderer (submission only) |
| `Project` | Project file and settings | Core |
| `ImGUI` | ImGui layer | Core, Renderer |

Applications include `Hydro.h` and should not include `Platform/Vulkan` headers directly.

## The frame

`Application::Run` does the same steps every frame:

1. **Events**: `Window::OnUpdate` polls GLFW; events go to the application, then to layers from the top down.
2. **UI**: `ImGuiLayer::Begin`, every layer's `OnImGuiRender`, then `ImGuiLayer::End`.
3. **Update**: every layer's `OnUpdate`. A layer updates its scene here (scripts, its own systems) and submits draw calls (`SceneRenderer::RenderScene` → `Renderer2D::DrawQuad`). Nothing touches the GPU yet.
4. **Render**: `Renderer::RenderFrame` records and submits the whole frame in **one** command buffer:
   1. wait for this frame-in-flight's fence, acquire a swapchain image;
   2. `Renderer2D::Render` — the offscreen 2D pass (batched quads);
   3. the swapchain pass — ImGui draws the editor UI, including the viewport that samples the 2D image;
   4. submit, present.
5. **Platform windows**: ImGui windows dragged outside the main window are drawn.

While the window is minimised, step 4 is skipped and the queued quads are discarded.

## Vulkan ownership

| Object | Owns |
| --- | --- |
| `VulkanRendererContext` | Instance, debug messenger (Debug builds), window surface, device |
| `VulkanDevice` | Logical device, graphics + present queues, upload command pool |
| `VulkanSwapChain` | Swapchain images, views, framebuffers, present render pass, render-finished semaphores; acquire/present; `Recreate` |
| `VulkanFrameContext` | Per frame in flight (`MAX_FRAMES_IN_FLIGHT = 2`): command pool + buffer, fence, image-acquired semaphore |
| `Renderer` | The above, and runs the frame |
| `VulkanRenderPass` | An offscreen colour target: one image + framebuffer per frame in flight |

Rules that follow from this:

- **Anything the CPU writes per frame exists once per frame in flight** (vertex buffers, uniform buffers, offscreen images). Index it with `Renderer::GetRenderFrame()`.
- **Record into the frame's command buffer** (`Renderer::GetCurrentCommandBuffer()` or the one passed to you). Don't create a separate submit for per-frame work.
- **Window resize rebuilds only the swapchain.** The editor's offscreen target follows the viewport panel (`Renderer2D::SetViewportSize`).
- **One-off uploads** (textures, static buffers) use `VulkanCommandBuffer`, which takes the device's upload pool and waits for completion.

## Code conventions

- `Ref<T>` (shared) and `Scope<T>` (unique) from `Core/Base.h`; create with `CreateRef` / `CreateScope`.
- Members `m_Name`, statics `s_Name`, types and methods `PascalCase`.
- No logical-not operator: write `== false` for booleans and `== nullptr` for pointers.
- Check Vulkan results with `VK_CHECK_RESULT(...)`.
- Includes use forward slashes (`#include "Hydro/Core/Base.h"`), and every header has `#pragma once`.

## Known gaps

These are where the code does not yet match the rules above. Fix them when you touch the area rather than copying the pattern. Every open problem is a [GitHub issue](https://github.com/joelnl74/Hydro/issues).

- `Renderer2D` includes `imgui_impl_vulkan.h` to hand the viewport image to ImGui.
- `Scene::m_Registry` is public; the editor reads it directly ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- Most Vulkan wrappers still have a manual `Destroy()` instead of a destructor (see [Resource creation](ResourceCreation.md#ownership)).
- `VkInstance` and the device are `static` members of `VulkanRendererContext` ([#50](https://github.com/joelnl74/Hydro/issues/50)).
