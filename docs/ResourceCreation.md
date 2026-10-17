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
| `Config` | Settings that are **saved** to disk and edited by users | In/out, serialized | `ProjectConfig` |

For example, `ImageSpecification` describes the image resource, while `TextureProperties` describes how a texture made from it is sampled.

`WindowProps` predates this table; it is a `Specification` in role. New types follow the table.

## Rules

1. **Plain aggregate.** Public fields with default values, no logic, no constructors beyond what aggregate initialisation gives you. A default-constructed spec must create something valid.
2. **Constructor takes it by `const&`** and keeps a copy if it needs one later. Expose it with `GetSpecification()` when callers may need it.
3. **Engine-level specs are backend-neutral.** Specs in `Renderer/` use engine enums (`ImageFormat`, `TextureFilter`, ...). Vulkan types (`VkRenderPass`, `VkFormat`) only appear in specs under `Platform/Vulkan/`.
4. **Translate in one place.** The backend converts engine enums to Vulkan ones in a single static function (`VulkanImage::GetImageFormat`, `ShaderDataTypeToVulkanFormat`).
5. **Immutable after creation.** To change what a resource is, create a new one or call a dedicated rebuild function (`VulkanRenderPass::Resize`). Don't reach into the spec and mutate it.
6. **Split what and how.** If a field changes how the resource is *used* and not what it *is*, it belongs in `Properties`, not `Specification`.

## Ownership

The object created from a spec owns its GPU handles:

- Its destructor frees them, and it is non-copyable (copy constructor and copy assignment deleted). Hold it in a `Scope<T>`, or a `Ref<T>` when it's shared.
- Destroy it only after the GPU is done with it: wait on the frame fence or call `Renderer::WaitIdle()`.

`VulkanImage` and `VulkanRenderPass` follow this. Older wrappers (`VulkanPipeline`, `VulkanShader`, buffers, `VullkanTexture`) still use a manual `Destroy()`. Move them over when you change them.

## Current inventory

| Struct | Used by | Status |
| --- | --- | --- |
| `ImageSpecification` (`Renderer/Image.h`) | `VulkanImage` | In use |
| `TextureProperties` (`Renderer/Image.h`) | — | Defined, not consumed yet. `VullkanTexture` takes an empty `VulkanTextureSpecification` and hard-codes its sampler. Next step: textures take `ImageSpecification` + `TextureProperties` ([#34](https://github.com/joelnl74/Hydro/issues/34)). |
| `ShaderSpecification`, `ShaderInformation` | `VulkanShader` | In use |
| `PipelineSpecification` | `VulkanPipeline` | In use; `Topology`, culling, depth and wireframe fields are not applied yet ([#32](https://github.com/joelnl74/Hydro/issues/32)) |
| `VulkanImageInfo` | `VulkanImage` | Output |
| `WindowProps` | `Window::Create` | In use |
| `ProjectConfig` | `Project` | In use |

## Adding a new resource

1. Add `FooSpecification` (and `FooProperties` if it has usage settings) next to the engine-facing API. Default every field.
2. Add the backend class under `Platform/Vulkan/` with `explicit VulkanFoo(const FooSpecification& spec)`.
3. Convert engine enums to Vulkan in one static helper.
4. Give it a destructor, delete its copy operations, and free everything in reverse creation order.
5. If it is written every frame, create one per frame in flight.
6. Add it to the inventory above.
