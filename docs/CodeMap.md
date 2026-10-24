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
| `Platform/Windows/*` | `WindowsWindow` (GLFW), `WindowsInput`, `WindowsUtilities` (`FileDialogs`) |
| **Asset** | |
| `Asset/Asset.*` | `AssetHandle` (= `UUID`), `AssetType` (to/from string, from a file extension), `Asset` base class (`Texture2D`, `SpriteAnimationSet`) |
| `Asset/AssetMetadata.h`, `Asset/AssetRegistry.*` | What is known about one asset; the handle ↔ path table (in memory, rebuilt by scanning) |
| `Asset/AssetMetaSerializer.*` | Where a file keeps its handle: first line of scenes/prefabs/`.hyanim`, `<file>.hymeta` sidecar for images (shared with the texture import config) |
| `Asset/AssetManager.*` | `AssetManager` subsystem: project open/close, scan (new handles, copies, missing files), `ImportAsset`, `BindHandle`, lookups; texture cache: `GetAsset<T>`, `GetLoadedAsset<T>`, `ReloadAsset<T>`, `GetLoadState` |
| **Renderer** (engine-facing, no Vulkan types in new headers) | |
| `Renderer/Renderer.*` | `Renderer` subsystem: owns context, swapchain, frames; `RenderFrame`; deletion queue (`SubmitResourceFree`); `Get/SetSettings`, `GetCapabilities` |
| `Renderer/RendererSettings.h` | `RendererSettings` (live, saved in `.hyproj`), `RendererCapabilities` |
| `Renderer/Renderer2D.*` | Quad batching, texture slots + flushes, per-view passes, stats, hot reload, wireframe rebuild; `DrawQuad` with tiling or a `TextureRect` (sprite of a sheet); `GetImGuiTexture` for editor previews; also implements `RenderView` |
| `Renderer/RenderView.h` | One offscreen picture of a scene (target, camera, batches) |
| `Renderer/Texture2D.*` | Texture resource (`ImageSpecification` + `TextureProperties`), `Create` (uploads an `ImportedTexture`), `LoadFromFile` (asks the `AssetManager` for project files, shares other files while alive), sprite list (`GetSprite`, `FindSprite`, `GetSpriteUVRect`), `TextureRect` |
| `Renderer/Image.h` | `ImageSpecification`, `TextureProperties`, image/texture enums |
| `Renderer/TextureImport.h` | `TextureImportConfig` (saved per image in `<image>.hymeta`), `SpriteMode`, `SpriteRegion`, `SpriteSlicingConfig` |
| `Renderer/TextureImporter.*` | Decodes an image file with its `.hymeta` (colour key, slicing) into `ImportedTexture` pixels + spec + config; compiles stb_image's implementation (`STB_IMAGE_IMPLEMENTATION`) for the engine; no GPU |
| `Renderer/TextureImportSerializer.*` | `.hymeta` read/write |
| `Renderer/SpriteSlicer.*` | Sprite sheet slicing: `SliceGrid`, `DetectGrid` (pure, no GPU) |
| `Renderer/SceneRenderer.*` | Submits a `Scene`'s sprites through the editor camera or the primary camera (sheet sprites: UV rect, pivot, flip) |
| `Renderer/Camera.*`, `SceneCamera.h`, `EditorCamera.*` | Projections (`*_ZO`, Y flip), per-scene camera data, editor orbit camera + `EditorCameraSettings` |
| `Renderer/Renderer3D.*` | Placeholder part for the 3D path (R9): `Init` / `Shutdown` only |
| **Platform/Vulkan** (backend; applications never include these) | |
| `VulkanRendererContext.*`, `VulkanDevice.*`, `VulkanPhysicalDevice.*` | Instance, surface, device (reached with `Renderer::GetVulkanDevice()`), queues, enabled features, `SubmitImmediate` for one-off uploads |
| `VulkanSwapChain.*`, `VulkanFrameContext.*` | Swapchain + present pass; per-frame command buffer, fence, semaphores (`MAX_FRAMES_IN_FLIGHT = 2`) |
| `VulkanAllocator.*` | VMA wrapper, GPU memory stats |
| `VulkanImage.*` | Image + view + sampler (render targets, textures from pixels) |
| `VulkanTexture2DData.h` | Backend half of `Texture2D` (`GetTexture2DData`) |
| `VulkanRenderPass.*` | Offscreen colour target, one image + framebuffer per frame in flight, `Resize` |
| `VulkanPipeline.*` | Pipeline from `PipelineSpecification` (shader and descriptor sets not owned); `BindDescriptorSet` |
| `VulkanShader.*` | shaderc compile from GLSL, `#include`, source list for hot reload |
| `VulkanDescriptorBuilder.*` | Layout + pool + one set per frame; `BindBuffer`/`BindImage`/`WriteImages` |
| `VulkanVertexBuffer.*`, `VulkanIndexBuffer.*`, `VulkanUniformBuffer.*` | Buffers; vertex layout types live in `VulkanVertexBuffer.h` |
| `VulkanUtils.h` | `VK_CHECK_RESULT` |
| **Scene** | |
| `Scene/Scene.*` | Registry, `CreateEntity`, `OnUpdateRuntime`/`OnUpdateEditor`, `Copy` (play mode), scripts, primary camera |
| `Scene/Entity.*` | Handle (`entt::entity` + `Scene*`), component access |
| `Scene/Components/Components.h` | Engine components (incl. `SpriteAnimatorComponent`) + `CopyableComponents` |
| `Scene/SpriteAnimation.*` | `SpriteAnimationSet` resource and `Asset` (named clips over one sheet), clip/frame/event types, `LoadFromFile` (shared by file, binds its asset handle) |
| `Scene/SpriteAnimationSerializer.*` | `.hyanim` read/write (handle first line, texture by `TextureHandle` + path) |
| `Scene/SpriteAnimator.*` | `UpdateSpriteAnimations` system; `SpriteAnimator` API (`Play`, `Stop`, `Pause`, `Resume`, `IsPlaying`...) |
| `Scene/ScriptableEntity.h`, `ScriptRegistry.*` | `NativeScriptComponent`, script base class, name → factory table |
| `Scene/EntitySerializer.*` | YAML for every component (both directions); used by scenes and prefabs |
| `Scene/SceneSerializer.*`, `PrefabSerializer.*` | `.hyscene` / `.hyprefab` files |
| **Project, Utils** | |
| `Project/Project.*`, `ProjectSerializer.*` | Active project, `ProjectConfig` (`.hyproj`) |
| `Utils/YAMLConversions.*`, `Utils/PlatformUtils.h` | glm ↔ YAML; `FileDialogs` |
| `Hydro.h` | What applications include |

## Editor: `Oxygen/src`

| File | Owns / does |
| --- | --- |
| `OxygenEditorApplication.cpp` | `CreateApplication`, registers example scripts, pushes `EditorLayer` |
| `EditorLayer.*` | Editor scene + play copy, the two `RenderView`s, `EditorCamera`, all windows, menus and top bar, open/save project and scene, asset drops, inspected asset, re-import of textures / reload of animation sets into the open scenes, galaxy demo |
| `EditorSettings.*` | `EditorSettings` + `EditorSettingsSerializer` (`EditorSettings.yaml`, session) |
| `EditorPaths.h` | Project-aware paths (scenes, prefabs, settings) |
| `EditorWindows/InspectorWindow.*` | Component UI, **Add Component**, script list, prefab button; the selected asset when no entity is selected (image import settings, `.hyanim` summary) |
| `EditorWindows/TextureImportPanel.*` | Inspector panel for an image: preview, `TextureImportConfig` (sprite mode, pixels per unit, filter, wrap, colour space, colour key), Apply / Revert |
| `EditorWindows/SpriteEditorWindow.*` | **Sprite Editor**: slice a sheet (grid, detect grid, manual), row names, per-sprite name / rect / pivot, sprites drawn over the image |
| `EditorWindows/SpriteAnimationWindow.*` | **Animation**: edit a `.hyanim` (sheet, clips, frames, durations, loop, flip, events, default clip), live preview, sprite picker; `MakeClipsFromSpriteNames` |
| `EditorWindows/PropertyUI.h` | Shared label/value rows, number fields and `SpriteImage` for the asset editors |
| `EditorWindows/HierachyWindow.*` | Entity list, create/delete, prefab drops |
| `EditorWindows/ProjectWindow.*` | Asset browser, drag sources (prefabs, scenes, images, `.hyanim`), prefab previews, image thumbnails; selects / opens assets; **Create Sprite Animation** from a sheet; hides `.hymeta`; calls `AssetManager::Rescan` when the folder changes, handle in tooltips |
| `EditorWindows/ConsoleWindow.*` | Log view |
| `EditorWindows/StatsWindow.*`, `RendererStatsWindow.*`, `StatsUI.h` | Frame timings; `Renderer2D` stats; shared table helpers |
| `EditorWindows/RenderSettingsWindow.*`, `ProjectSettingsWindow.*`, `EditorSettingsWindow.*` | Live `RendererSettings`; project name/start scene; editor preferences pages |
| `EditorWindows/EditorStyle.*`, `Utilities/EditorChrome.*` | Themes from one accent colour; droplet, waterline, icon buttons, play tint |
| `Utilities/AssetDragDrop.h` | Drag & drop payload (file path) |
| `Scripts/ExampleScripts.h` | `SpinScript`, `BobScript`, `VillagerWalkScript` (WASD + `SpriteAnimator::Play`) |
| `assets/shaders/vertex.vert`, `fragment.frag` | The quad shader (`inTextures[32]` must match `Renderer2D`) |
| `project/` | The Sandbox project Oxygen opens at startup; `Assets/Characters` (villager sheet, `.hymeta`, `.hyanim`) + `Scenes/Villagers.hyscene` show sprite animation |

Dead files (delete when convenient): `Core/DeltaTime.h`, `Core/LayerController.*`, `Oxygen/src/EditorWindows/ContentWindow.h`. Also `Platform/Vulkan/VullkanTexture.*`, `VulkanBuffer.*` and `VulkanRenderCommandBuffer.*`: emptied on Oct 20, 2026, the files themselves still need deleting.

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
| New asset type | `AssetType` + `AssetTypeToString` / `AssetTypeFromString` / `GetAssetTypeFromExtension` (`Asset.cpp`); `AssetMetaSerializer::StoresHandleInFile` if Hydro writes the file (and its serializer writes `Handle` first); a loaded type derives from `Asset` |
| Sprite sheets / import settings | `TextureImport.h`, `TextureImportSerializer.*`, `TextureImporter.*`, `SpriteSlicer.*`, `Texture2D.cpp` (`Create`, `SetImportConfig`) |
| Sprite animation | `SpriteAnimation.*`, `SpriteAnimationSerializer.*`, `SpriteAnimator.*` (system + API), `Scene.cpp` (`OnUpdateRuntime`), inspector section, `SpriteAnimationWindow.*` |
| An image shown in the editor | `Renderer2D::GetImGuiTexture(texture)` (pooled ImGui handles), `PropertyUI::SpriteImage` for one sprite |
| Docs | Edit `docs/`, then `python scripts/regen_wiki.py`; wiki-only pages (Components, Roadmap, Known-Issues) by hand |

## Gotchas

- **Old EnTT** (vendored single header `Hydro/vendor/entt/entt.hpp`, no version macro): `registry.has<T>`, `view.get<A, B>(e)`; no `all_of`/`any_of` (upgrade is Roadmap E2).
- **Line endings**: CRLF in the working copy for sources and docs (`.gitattributes`); `*.sh` is LF.
- **Style**: `namespace Hydro` only, no `!` (use `== false` / `== nullptr`), braces on every body, `m_PascalCase`. See the skill or [Architecture](Architecture.md#code-conventions).
- `Renderer2D.h`, `Renderer.h`, `RenderView.h` still include `vulkan.h` (A7); don't add Vulkan to other `Renderer/` headers.
- Shaders are compiled at runtime by shaderc from `assets/shaders/...`, relative to the working directory (run Oxygen from the `Oxygen/` folder).

## Checking without Windows

`scripts/check.sh [files...]`: g++ `-fsyntax-only -Wall -Wextra` over the engine and editor sources (Windows-only files skipped) and glslang over the shaders. Linux/WSL; downloads Vulkan-Headers, shaderc headers and builds glslang once into `/tmp/hydro-check`. Not a substitute for an MSVC `/W4` build.
