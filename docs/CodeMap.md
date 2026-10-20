# Code map

Where things live and which files a typical change touches. Read this first; open the full docs only for the section you need (links below). Keep it short: one line per file, update it when you add, rename or delete a file.

Rules and reasons: [Architecture](Architecture.md), [Subsystems](Subsystems.md), [Resource creation](ResourceCreation.md), [Scene, ECS and systems](SceneAndECS.md). Unfinished work is tagged in code: search `TODO(assets)` (and `Known gaps` in each doc).

## Engine: `Hydro/src/Hydro`

| File | Owns / does |
| --- | --- |
| **Core** | |
| `Core/Application.*` | Subsystem `Init`/`Shutdown` order, the frame loop, `LayerStack`; `ApplicationSpecification` |
| `Core/EntryPoint.h` | `main`: `CreateApplication` → `Init` → `Run` → `Shutdown` |
| `Core/Base.h` | `Ref`/`Scope`, `CreateRef`/`CreateScope`, `BIT`, debug break |
| `Core/Assert.h`, `Core/Log.*` | `HY_CORE_ASSERT`; `Log` subsystem (spdlog, file, in-memory history for the Console) |
| `Core/Time.*` | `Time` subsystem: delta time, frame count, `FramePhase` timings |
| `Core/Layer.*`, `Core/LayerStack.*` | Layers (`OnAttach`/`OnDetach`/`OnUpdate`/`OnImGuiRender`) and their stack |
| `Core/Window.*`, `Core/Input.h`, `Core/KeyCodes.h` | Window interface + `WindowSpecification`; stateless input polling |
| `Core/UUID.*`, `Core/FileWatcher.*` | 64-bit IDs; file change polling (shader hot reload) |
| `Events/*` | Event types, `EventDispatcher` subsystem |
| `ImGUI/ImGuiLayer.*` | ImGui init/shutdown, fonts (`ImGuiLayerSpecification`), present-pass callback |
| `Platform/GLFW/*` | `GlfwWindow` (every desktop OS; sizes in framebuffer pixels), `GlfwInput` (`Input`), ImGui's GLFW backend (`imgui_impl_glfw.*`, vendored) |
| `Platform/Windows/WindowsFileDialogs.cpp`, `Platform/MacOS/MacOSFileDialogs.cpp`, `Platform/Linux/LinuxFileDialogs.cpp` | `FileDialogs` per OS (Win32, AppleScript, zenity); premake builds only the target's folder |
| **Renderer** (engine-facing, no Vulkan types in new headers) | |
| `Renderer/Renderer.*` | `Renderer` subsystem: owns context, swapchain, frames; `RenderFrame`; deletion queue (`SubmitResourceFree`); `Get/SetSettings`, `GetCapabilities` |
| `Renderer/RendererSettings.h` | `RendererSettings` (live, saved in `.hyproj`), `RendererCapabilities` |
| `Renderer/Renderer2D.*` | Quad batching, texture slots + flushes, per-view passes, stats, hot reload, wireframe rebuild; also implements `RenderView` |
| `Renderer/RenderView.h` | One offscreen picture of a scene (target, camera, batches) |
| `Renderer/Texture2D.*` | Texture resource (`ImageSpecification` + `TextureProperties`), `LoadFromFile` (shared by file); compiles stb_image's implementation (`STB_IMAGE_IMPLEMENTATION`) for the engine |
| `Renderer/Image.h` | `ImageSpecification`, `TextureProperties`, image/texture enums |
| `Renderer/SceneRenderer.*` | Submits a `Scene`'s sprites through the editor camera or the primary camera |
| `Renderer/Camera.*`, `SceneCamera.h`, `EditorCamera.*` | Projections (`*_ZO`, Y flip), per-scene camera data, editor orbit camera + `EditorCameraSettings` |
| `Renderer/Renderer3D.*` | Placeholder part for the 3D path (R9): `Init` / `Shutdown` only |
| **Platform/Vulkan** (backend; applications never include these) | |
| `VulkanRendererContext.*`, `VulkanDevice.*`, `VulkanPhysicalDevice.*` | Instance (portability enumeration for macOS), surface, device (reached with `Renderer::GetVulkanDevice()`; portability subset when offered), queues, enabled features, `SubmitImmediate` for one-off uploads |
| `VulkanSwapChain.*`, `VulkanFrameContext.*` | Swapchain + present pass; per-frame command buffer, fence, semaphores (`MAX_FRAMES_IN_FLIGHT = 2`) |
| `VulkanAllocator.*` | VMA wrapper, GPU memory stats |
| `VulkanImage.*` | Image + view + sampler (render targets, textures from pixels) |
| `VulkanTexture2DData.h` | Backend half of `Texture2D` (`GetTexture2DData`) |
| `VulkanRenderPass.*` | Offscreen colour target, one image + framebuffer per frame in flight, `Resize` |
| `VulkanPipeline.*` | Pipeline from `PipelineSpecification` (shader and descriptor sets not owned); `BindDescriptorSet` |
| `VulkanShader.*` | shaderc compile from GLSL, `#include`, `Defines` (macros), source list for hot reload |
| `VulkanDescriptorBuilder.*` | Layout + pool + one set per frame; `BindBuffer`/`BindImage`/`WriteImages` |
| `VulkanVertexBuffer.*`, `VulkanIndexBuffer.*`, `VulkanUniformBuffer.*` | Buffers; vertex layout types live in `VulkanVertexBuffer.h` |
| `VulkanUtils.h` | `VK_CHECK_RESULT` |
| **Scene** | |
| `Scene/Scene.*` | Registry, `CreateEntity`, `OnUpdateRuntime`/`OnUpdateEditor`, `Copy` (play mode), scripts, primary camera |
| `Scene/Entity.*` | Handle (`entt::entity` + `Scene*`), component access |
| `Scene/Components/Components.h` | Engine components + `CopyableComponents` |
| `Scene/ScriptableEntity.h`, `ScriptRegistry.*` | `NativeScriptComponent`, script base class, name → factory table |
| `Scene/EntitySerializer.*` | YAML for every component (both directions); used by scenes and prefabs |
| `Scene/SceneSerializer.*`, `PrefabSerializer.*` | `.hyscene` / `.hyprefab` files |
| **Project, Utils** | |
| `Project/Project.*`, `ProjectSerializer.*` | Active project, `ProjectConfig` (`.hyproj`) |
| `Utils/YAMLConversions.*`, `Utils/PlatformUtils.*` | glm ↔ YAML; `FileDialogs` interface and its filter parsing (the dialogs are in `Platform/<OS>`) |
| `Core/PlatformDetection.h` | `HY_PLATFORM_WINDOWS` / `HY_PLATFORM_MACOS` / `HY_PLATFORM_LINUX` |
| `Hydro.h` | What applications include |

## Editor: `Oxygen/src`

| File | Owns / does |
| --- | --- |
| `OxygenEditorApplication.cpp` | `CreateApplication`, registers example scripts, pushes `EditorLayer` |
| `EditorLayer.*` | Editor scene + play copy, the two `RenderView`s, `EditorCamera`, all windows, menus and top bar, open/save project and scene, asset drops, galaxy demo |
| `EditorSettings.*` | `EditorSettings` + `EditorSettingsSerializer` (`EditorSettings.yaml`, session) |
| `EditorPaths.h` | Project-aware paths (scenes, prefabs, settings) |
| `EditorWindows/InspectorWindow.*` | Component UI, **Add Component**, script list, prefab button |
| `EditorWindows/HierachyWindow.*` | Entity list, create/delete, prefab drops |
| `EditorWindows/ProjectWindow.*` | Asset browser, drag sources (prefabs, scenes, images), prefab previews |
| `EditorWindows/ConsoleWindow.*` | Log view |
| `EditorWindows/StatsWindow.*`, `RendererStatsWindow.*`, `StatsUI.h` | Frame timings; `Renderer2D` stats; shared table helpers |
| `EditorWindows/RenderSettingsWindow.*`, `ProjectSettingsWindow.*`, `EditorSettingsWindow.*` | Live `RendererSettings`; project name/start scene; editor preferences pages |
| `EditorWindows/EditorStyle.*`, `Utilities/EditorChrome.*` | Themes from one accent colour; droplet, waterline, icon buttons, play tint |
| `Utilities/AssetDragDrop.h` | Drag & drop payload (file path) |
| `Scripts/ExampleScripts.h` | `SpinScript`, `BobScript` |
| `assets/shaders/vertex.vert`, `fragment.frag` | The quad shader (`inTextures[HY_TEXTURE_SLOTS]`, 32 or 16, set by `Renderer2D`) |
| `project/` | The Sandbox project Oxygen opens at startup |

Dead files (delete when convenient): `Core/DeltaTime.h`, `Oxygen/src/EditorWindows/ContentWindow.h`.

## Build files and scripts

| File | Owns / does |
| --- | --- |
| `premake5.lua` | Workspace, configurations, architecture per OS (ARM64 on macOS), includes every project |
| `Dependencies.lua` | Include dirs and libraries per OS (Vulkan SDK from `VULKAN_SDK`), `ExternalIncludeDirs`, `LinkHydroDependencies()` for applications on macOS/Linux |
| `Hydro/premake5.lua`, `Oxygen/premake5.lua` | The engine and editor projects; per-OS filters (`Platform/<OS>` folders, links, shader build step, shaderc DLL copy on Windows) |
| `Hydro/vendor/GLFW.lua`, `Hydro/vendor/ImGui.lua`, `Hydro/vendor/yaml-cpp/premake5.lua` | Vendored library projects (GLFW and ImGui are submodules, so their projects live next to them) |
| `scripts/Setup.bat`, `scripts/Setup.sh` → `Setup.py` | First setup (Windows / macOS + Linux): premake download, Vulkan SDK check, submodules, project files |
| `scripts/Win-GenProjects.bat`, `Mac-GenProjects.sh`, `Linux-GenProjects.sh` | Regenerate project files (Visual Studio; Makefiles and optionally Xcode; Makefiles) |
| `scripts/Build.sh`, `scripts/Run.sh` | macOS/Linux: build a configuration with make; run Oxygen from `Oxygen/` |
| `scripts/check.sh`, `scripts/regen_wiki.py` | Syntax and shader check without a build; mirror `docs/` to the wiki |

## Recipes: which files a change touches

| Change | Files |
| --- | --- |
| New engine component | `Components.h` (+ `CopyableComponents`), `EntitySerializer.cpp` (both directions, defaults), `InspectorWindow.cpp` (`DrawComponents` + **Add Component**), wiki `Components.md`, `docs/SceneAndECS.md` table |
| New field on a component | Same as above minus `CopyableComponents` |
| New engine system | Function over a view, called from `Scene::OnUpdateEditor` or `OnUpdateRuntime` (`Scene.cpp`); frame order in `docs/SceneAndECS.md` |
| New renderer setting | `RendererSettings.h`, read in `Renderer2D.cpp`/`Renderer.cpp` (or applied in `Renderer::SetSettings`), `ProjectSerializer.cpp`, `RenderSettingsWindow.cpp`, docs inventory |
| New editor preference | `EditorSettings.h`, `EditorSettings.cpp` (serializer, default on missing key), `EditorSettingsWindow.cpp` |
| New editor window | `EditorWindows/<Name>Window.*`, member + View menu entry in `EditorLayer.*`; callbacks for anything that saves |
| New resource / spec | Spec next to the API (`Renderer/`), backend in `Platform/Vulkan/` (destructor frees through `Renderer::SubmitResourceFree`, owner holds a `Scope`), inventory in `docs/ResourceCreation.md` |
| New subsystem | Class + `Init`/`Shutdown`, `Application.cpp` (mirrored positions), tables in `docs/Subsystems.md` |
| 2D drawing / batching | `Renderer2D.*`, shaders in `Oxygen/assets/shaders`, `RendererStatsWindow.cpp` for new stats |
| Textures | `Texture2D.*`, `VulkanImage.*`, `VulkanTexture2DData.h`; sprites: `SceneRenderer.cpp`, `EntitySerializer.cpp`, `InspectorWindow.cpp` |
| Docs | Edit `docs/`, then `python scripts/regen_wiki.py`; wiki-only pages (Components, Roadmap, Known-Issues) by hand |
| OS-specific code | `Platform/<OS>/<OS><Name>.cpp` for each OS (Windows, MacOS, Linux) behind a platform-free interface; shared desktop code in `Platform/GLFW`; per-OS build settings in the `filter "system:..."` blocks of the premake files; `docs/Architecture.md#platforms` |

## Gotchas

- **Old EnTT** (vendored single header `Hydro/vendor/entt/entt.hpp`, no version macro): `registry.has<T>`, `view.get<A, B>(e)`; no `all_of`/`any_of` (upgrade is Roadmap E2).
- **Line endings**: CRLF in the working copy for sources and docs (`.gitattributes`); `*.sh` is LF.
- **Style**: `namespace Hydro` only, no `!` (use `== false` / `== nullptr`), braces on every body, `m_PascalCase`. See the skill or [Architecture](Architecture.md#code-conventions).
- `Renderer2D.h`, `Renderer.h`, `RenderView.h` still include `vulkan.h` (A7); don't add Vulkan to other `Renderer/` headers.
- Shaders are compiled at runtime by shaderc from `assets/shaders/...`, relative to the working directory (run Oxygen from the `Oxygen/` folder).

## Checking without Windows

`scripts/check.sh [files...]`: g++ `-fsyntax-only -Wall -Wextra` over the engine and editor sources (`Platform/Windows` skipped) and glslang over the shaders. Linux/WSL; downloads Vulkan-Headers, shaderc headers and builds glslang once into `/tmp/hydro-check`. Not a substitute for an MSVC `/W4` build.

A real build on macOS or Linux: `scripts/Mac-GenProjects.sh` (or `Linux-GenProjects.sh`), then `scripts/Build.sh` and `scripts/Run.sh` (see [Platforms](Architecture.md#platforms)).
