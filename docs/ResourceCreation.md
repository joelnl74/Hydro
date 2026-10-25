# Resource creation

Every resource in Hydro (an image, a shader, a pipeline, a window, a project) is created from a plain struct that describes it. The struct is filled in, then passed to the constructor. Nothing is configured by calling setters afterwards.

```cpp
ImageSpecification spec;
spec.Width  = 1280;
spec.Height = 720;
spec.Format = ImageFormat::SRGB;
spec.Usage  = ImageUsage::Attachment;

Scope<VulkanImage> target = CreateScope<VulkanImage>(spec);
```

## The pattern

This is the **Parameter Object** pattern, also called **Resource Descriptor** or **Configuration Object**. It is the same idea as Vulkan's own `Vk*CreateInfo` structs and D3D12's `*_DESC`.

The name `ImageSpecification` is *not* the Specification pattern from domain-driven design. That pattern evaluates whether an object meets a set of criteria. Here a specification just describes the resource to create.

## Naming

The suffix says what role the struct plays.

| Suffix | Role | Direction | Example |
| --- | --- | --- | --- |
| `Specification` | **What** the resource is: size, format, usage, inputs | In, to the constructor | `ImageSpecification`, `PipelineSpecification`, `ShaderSpecification` |
| `Properties` | **How** it behaves when used: sampling, wrapping, mips | In, to the constructor | `TextureProperties` |
| `Info` | Handles and facts **produced** by creation | Out, read-only | `VulkanImageInfo` |
| `Settings` | **Live** options of a subsystem, changed while running; applied from the next frame | In, through `Set<...>Settings` | `RendererSettings` |
| `Capabilities` | What the hardware supports, found at `Init` | Out, read-only | `RendererCapabilities` |
| `Config` | Settings that are **saved** to disk and edited by users | In/out, serialized | `ProjectConfig` (holds a `RendererSettings`) |

For example, `ImageSpecification` describes the image resource, while `TextureProperties` describes how a texture made from it is sampled.

## Rules

1. **Plain aggregate.** Public fields with default values, no logic, no constructors beyond what aggregate initialisation gives you. A default-constructed spec must create something valid.
2. **Constructor takes it by `const&`** and keeps a copy if it needs one later. Expose it with `GetSpecification()` when callers may need it.
3. **Engine-level specs are backend-neutral.** Specs in `Renderer/` use engine enums (`ImageFormat`, `TextureFilter`, ...). Vulkan types (`VkRenderPass`, `VkFormat`) only appear in specs under `Platform/Vulkan/`.
4. **Translate in one place.** The backend converts engine enums to Vulkan ones in a single static function (`VulkanImage::GetImageFormat`, `ShaderDataTypeToVulkanFormat`).
5. **Immutable after creation.** To change what a resource is, create a new one or call a dedicated rebuild function (`VulkanRenderTarget::Resize`). Don't reach into the spec and mutate it.
6. **Split what and how.** If a field changes how the resource is *used* and not what it *is*, it belongs in `Properties`, not `Specification`.

## Ownership

The object created from a spec owns its GPU handles:

- Its destructor frees them, and it is non-copyable (copy constructor and copy assignment deleted).
- Its single owner holds it in a `Scope<T>` and hands it to users as `T&` / `const T&`. Use `Ref<T>` only when several objects really share it (a `Texture2D` used by many sprites). Don't take a `Ref<T>` by value or by non-const reference to use it; take `const Ref<T>&` only when you keep a copy.
- Its destructor doesn't destroy the handles directly: it passes a function that does to `Renderer::SubmitResourceFree`, capturing the handles (never `this`). So it can be destroyed at any point in a frame, even while frames in flight still use it, and nobody calls `Renderer::WaitIdle()` to release something.

Every GPU resource follows this: `VulkanImage`, `VulkanRenderTarget`, `VulkanVertexBuffer`, `VulkanIndexBuffer`, `VulkanUniformBuffer`, `VulkanDescriptorBuilder`, `VulkanPipeline`, `VulkanShader` (frees its modules at once; pipelines don't need them after creation), `VulkanImageReadback` (no specification: its host buffers are made on the first `Record` and reused) and `Texture2D`. There are no `Ref<Vulkan...>` members and no manual `Destroy()` methods left (Roadmap A3, A4, done Oct 20, 2026).

**Deletion queue.** `Renderer::SubmitResourceFree(std::function<void()>)` stores the free function with the current frame number and runs it at the start of the frame `MAX_FRAMES_IN_FLIGHT` frames later, right after that frame slot's fence wait (the GPU is then done with every frame that could have used the resource). Everything still queued runs after a swapchain rebuild (the device is idle) and at `Renderer::Shutdown`, before the allocator and the device are destroyed. Frees run in the order they were queued, so an owner that releases children first (framebuffers before their images) keeps that order. A resource released after `Renderer::Shutdown` asserts and leaks its handles, so owners release GPU objects before then (layers in `OnDetach`).

A non-owning reference to another resource is a reference or a plain pointer documented as such: `PipelineSpecification::Shader` (only read while the pipeline is created) and `PipelineSpecification::DescriptorSets` (layouts read at creation; must outlive the pipeline if `BindDescriptorSets` is used), `VulkanSwapChain` and `VulkanFrameContext`'s `VulkanDevice*`.

This RAII rule is for resources only. Subsystems and their parts (`Renderer`, `VulkanRendererContext`, `VulkanFrameContext`, ...) are started and stopped explicitly with `Init` / `Shutdown` instead; see [Subsystems](Subsystems.md).

## Current inventory

| Struct | Used by | Status |
| --- | --- | --- |
| `ImageSpecification` (`Renderer/Image.h`) | `VulkanImage`, `Texture2D` | In use. `Mips` and `Layers` are ignored for textures (one level, one layer) |
| `RenderTargetSpecification` (`Renderer/RenderTarget.h`) | `VulkanRenderTarget` | In use (M1): size, colour formats, optional depth format, optional entity-ID attachment, debug name. Resized with `VulkanRenderTarget::Resize`. Views use one SRGB colour attachment; the 2D light buffer one RGBA16F |
| `RenderViewSpecification` (`Renderer/RenderView.h`) | `RenderView` | In use: initial size and a debug name. Resized with `RenderView::SetSize`, applied at its next render |
| `TextureProperties` (`Renderer/Image.h`) | `Texture2D`, `VulkanImage` (texture constructors) | In use (Oct 20, 2026): `SamplerFilter` and `SamplerWrap` build the sampler (`VulkanImage::GetFilter` / `GetAddressMode`). `GenerateMips` isn't applied yet (one mip level). sRGB is the image's `Format`, not a property. Render targets use clamp + linear |
| `Texture2D` (`Renderer/Texture2D.h`) | Sprites, `Renderer2D::DrawQuad` | In use: `Texture2D(ImageSpecification, pixels, TextureProperties)` or `Texture2D::LoadFromFile(path, properties, format)` (`nullptr` on failure; project files are shared by the `AssetManager`, others while alive), or `AssetManager::GetAsset<Texture2D>(handle)`. RGBA8 only. Shared through `Ref<Texture2D>`. TODO(assets): components hold the handle (Roadmap AS3) |
| `TextureImportConfig` (`Renderer/TextureImport.h`) | `Texture2D::LoadFromFile`, saved as `<image>.hymeta` by `TextureImportSerializer` | In use: format, sampling (`TextureProperties`), colour key, `SpriteMode`, pixels per unit, `SpriteSlicingConfig` (editor-side slicing settings) and `SpriteRegion`s. A `Config` (saved, edited by users). TODO(assets): becomes the texture's registry entry |
| `SpriteAnimationSetSpecification` (`Scene/SpriteAnimation.h`) | `SpriteAnimationSet` | In use: texture (`Ref<Texture2D>`, TODO(assets)), clips, default clip. Frame durations are clamped to `MinFrameDuration`. `SpriteAnimationSet::LoadFromFile` reads a `.hyanim` and shares sets by file. Immutable; shared through `Ref<SpriteAnimationSet>` |
| `ShaderSpecification`, `ShaderInformation` | `VulkanShader` | In use. `path` is GLSL source (compiled with shaderc on creation, `#include` relative to the file) or a `.spv` file. Throws with the compiler output when a stage fails. Owns its modules (destructor). |
| `PipelineSpecification` | `VulkanPipeline` | In use. All fields are applied, including `Blend` (`BlendMode`). `Shader` (`VulkanShader*`, not owned, only used during creation and null in `GetSpecification()` afterwards) and `DescriptorSets` (`VulkanDescriptorBuilder*`, optional, in set-index order) point at objects their owner keeps. `Wireframe` and `LineWidth` != 1 fall back with a warning when the GPU lacks `fillModeNonSolid` / `wideLines`. `ColorFormats` and `DepthFormat` (attachment formats for dynamic rendering, matching the target; integer formats are never blended) replace the render pass; `PushConstantSize` declares push constants for the vertex and fragment stages. Depth state has no effect while `DepthFormat` is `None`. |
| `VulkanImageInfo` | `VulkanImage` | Output |
| `WindowSpecification` (`Core/Window.h`) | `Window::Init` | In use (was `WindowProps`) |
| `ApplicationSpecification` (`Core/Application.h`) | `Application` constructor | In use: application name, main window and the ImGui layer's fonts |
| `ImGuiLayerSpecification`, `ImGuiFontSpecification` (`ImGUI/ImGuiLayer.h`) | `ImGuiLayer` constructor (from `ApplicationSpecification::ImGui`) | In use: fonts loaded into the atlas at startup (name, file, pixel size); the first is the default. Missing files are logged and skipped |
| `EditorCameraSettings` (`Renderer/EditorCamera.h`) | `EditorCamera::SetSettings` | In use: field of view, clip planes, orbit/pan/zoom speed multipliers. Live; Oxygen saves them in its editor settings |
| `ProjectConfig` | `Project`, saved by `ProjectSerializer` (`.hyproj`) | In use: name, asset directory, start scene, `RendererSettings` |
| `RendererSettings` (`Renderer/RendererSettings.h`) | `Renderer::SetSettings` | In use: clear colour, vsync, wireframe, shader hot reload, 2D lighting (`Lighting`, `LightFalloff` smooth or banded, `LightBands`), `Shadows` (blob shadows), `Features` (the project's `RenderFeatureSettings`) |
| `RenderFeatureSettings` (`Renderer/RenderFeatures.h`) | `RendererSettings::Features`, `SceneRenderSettings::Features`, `RenderView::SetRenderFeatures` | In use: depth, sprite sorting, lighting mode, default sprite alpha, meshes; `Preset2D()` / `Preset3D()`. Changing it rebuilds the view's render graph once; see [Render features](Architecture.md#render-features-and-the-render-graph) |
| `SceneRenderSettings` (`Renderer/RenderFeatures.h`) | `Scene::SetRenderSettings`, saved by `SceneSerializer` | In use: `OverrideProject` + `Features`, a scene's own render features (written to the `.hyscene` only when overriding) |
| `RendererCapabilities` (`Renderer/RendererSettings.h`) | `Renderer::GetCapabilities` | Output: GPU name and Vulkan version, required (dynamic rendering, synchronization2) and optional features as enabled, limits (push constants, sampled images per stage, anisotropy, depth formats); see [Device requirements](Architecture.md#device-requirements) |

## Adding a new resource

1. Add `FooSpecification` (and `FooProperties` if it has usage settings) next to the engine-facing API. Default every field.
2. Add the backend class under `Platform/Vulkan/` with `explicit VulkanFoo(const FooSpecification& spec)`.
3. Convert engine enums to Vulkan in one static helper.
4. Give it a destructor, delete its copy operations, and free everything in reverse creation order. Its owner holds it in a `Scope<T>`.
5. If it is written every frame, create one per frame in flight.
6. Add it to the inventory above.
