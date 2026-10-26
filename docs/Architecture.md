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
| 1. `Log` | 8. client `OnShutdown` |
| 2. `Time` | 7. wait for the GPU, detach layers (ImGui included) |
| 3. `EventDispatcher` | 6. `AssetManager` |
| 4. `Window` | 5. `Renderer` |
| 5. `Renderer` (context, swapchain, frames, allocator, `Renderer2D`, `Renderer3D`) | 4. `Window` |
| 6. `AssetManager` (empty until the application opens a project) | 3. `EventDispatcher` |
| 7. `ImGuiLayer` overlay | 2. `Time` |
| 8. client `OnInit` (pushes layers) | 1. `Log` |

The rules, the vocabulary (subsystem, part, layer, system, resource) and how to name new subsystems are in [Subsystems](Subsystems.md).

## Source layout (`Hydro/src/Hydro`)

| Folder | Responsibility | May depend on |
| --- | --- | --- |
| `Core` | Application, `LayerStack` and layers, window interface, logging (`Log`, which also keeps the last 5000 messages in memory for `Log::GetMessagesSince`; Oxygen's **Console** window shows them), frame clock (`Time`), asserts, input, UUID, `FileWatcher` | — |
| `Asset` | Asset identity: `AssetHandle`, `AssetType` and the `Asset` base (`Asset.h`), `AssetRegistry`, `AssetMetaSerializer` (handles inside scenes/prefabs and in `.hymeta` sidecars), the `AssetManager` subsystem. See [Asset system](#asset-system) | Core (yaml-cpp). `Renderer` (`Texture2D` is an `Asset`) and `Scene` (the serializers write handles) depend on it |
| `Events` | Event types and the dispatcher | Core |
| `Platform/Windows` | GLFW window, input, file dialogs | Core, Events |
| `Platform/Vulkan` | Everything that talks to Vulkan | Core; `Renderer.h` for the device (`Renderer::GetVulkanDevice()`), the deletion queue and the frame index |
| `Renderer` | Engine-facing rendering API (`Renderer`, `RendererSettings`, `Renderer2D`, `RenderView`, `Texture2D`, `SceneRenderer`, `Camera`, `SceneCamera`, `EditorCamera`, `Image.h`) | Core, Platform/Vulkan |
| `Scene` | Scene, entities, components, scripts and `ScriptRegistry`, scene copy for play mode, scene and prefab saving (yaml-cpp) | Core, Utils, Renderer (submission, and `Texture2D` for sprites). Until the asset system, `EntitySerializer` also loads sprite textures and reads `Project` for the asset folder (see [Asset system](#asset-system)) |
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
  StartSceneHandle: 1234567890123456  # asset handle of the scene opened first; absent = none
Renderer:
  ClearColor: [0, 0, 0, 1]
  VSync: true
  Wireframe: false
  ShaderHotReload: true
  Lighting: true            # 2D lighting on/off
  LightFalloff: Smooth      # Smooth or Banded
  LightBands: 8             # steps of a Banded falloff (2 to 32)
  Shadows: true             # blob shadows (Shadow2DComponent) in lit views
```

- `Project::Load(file)` / `Project::New(file)` make a project active; `Project::SaveActive()` writes it; `Project::Close()` releases it. `Project` doesn't call other subsystems: whoever opens a project applies its settings (`Renderer::SetSettings(config.Renderer)`) and, when saving, copies the live settings back (`config.Renderer = Renderer::GetSettings()`).
- `RendererSettings` are live: `Renderer::SetSettings` applies them from the next frame. Clear colour, hot reload and the lighting options are read every frame, wireframe rebuilds the 2D pipeline, and vsync recreates the swapchain. `Renderer::GetCapabilities()` says what the GPU supports (wireframe, wide lines).
- Oxygen opens the first `.hyproj` in its `project` folder at startup (`Oxygen/project/Sandbox.hyproj`). **View > Render Settings** edits the renderer settings live and **View > Project Settings** edits the name and start scene; **File > Save Project** (or the Save buttons) writes both.

## The frame

`Application::Run` does the same steps every frame, each timed by the `Time` subsystem as a `FramePhase`:

1. **Events**: `Window::OnUpdate` polls GLFW; events go to the application, then to layers from the top down.
2. **UI**: `ImGuiLayer::Begin`, every layer's `OnImGuiRender`, then `ImGuiLayer::End`.
3. **Update**: every layer's `OnUpdate(Time::GetDeltaTime())`. A layer updates its scene here (`Scene::OnUpdateRuntime` or `OnUpdateEditor`, then its own systems) and submits draw calls into a `RenderView` through a camera (`SceneRenderer::RenderRuntime` or `RenderEditor` → `Renderer2D::BeginScene(view, ...)` + `DrawQuad` + `EndScene`). Nothing touches the GPU yet.
4. **Render**: `Renderer::RenderFrame` records and submits the whole frame in **one** command buffer:
   1. wait for this frame-in-flight's fence, run the deletion queue's frees whose frames are now finished (`Renderer::SubmitResourceFree`, see [Vulkan ownership](#vulkan-ownership)), acquire a swapchain image;
   2. `Renderer2D::Render` — one offscreen pass per `RenderView` begun this frame, recorded stage by stage from the view's render graph (see [Render features and the render graph](#render-features-and-the-render-graph)) (its batched quads, seen through its camera), each cleared to `RendererSettings::ClearColor` (a view that has lights first gets a light buffer pass, see [2D lighting](#2d-lighting); GPU timestamps cover the whole 2D work and, per lit view (first 4), the light pass, shadows and composite, shown in the Stats window). It applies each view's pending resize before its pass, and first rebuilds its pipeline if `Wireframe` changed and, when hot reload is on, rebuilds its shader and pipeline if a shader source changed (see [Shaders](#shaders));
   3. the swapchain pass — ImGui draws the editor UI, including the Editor and Game windows, which sample their views' images;
   4. submit, present.

   The renderer reports steps 1–3 to `Time` as the `Render` phase and step 4 as `Present`.
5. **Platform windows**: ImGui windows dragged outside the main window are drawn.

While the window is minimised, step 4 is skipped and the queued quads are discarded.

## Cameras and clip space

Every projection matrix is built by `Camera::Perspective` / `Camera::Orthographic` (`Renderer/Camera.h`), so Vulkan's clip-space conventions are handled in one place: depth is `[0, 1]` (GLM's `*_ZO` functions; no `GLM_FORCE_DEPTH_ZERO_TO_ONE` define) and Y is flipped, so world +Y is up on screen. Because of the flip, geometry is counter-clockwise (`VulkanPipeline` uses `VK_FRONT_FACE_COUNTER_CLOCKWISE`) and the offscreen image is shown with ordinary UVs.

## Render views

A `RenderView` (`Renderer/RenderView.h`, created from a `RenderViewSpecification`) is one picture of a scene: an offscreen image (a `VulkanRenderTarget`), the camera it is seen through, and the quads queued for it this frame. Oxygen has two, shown in its **Editor** and **Game** windows with `ImGui::Image(view.GetImGuiTexture(), ...)`.

```cpp
Renderer2D::BeginScene(view, camera, cameraTransform); // or BeginScene(view, viewProjection)
Renderer2D::DrawQuad(transform, color);
Renderer2D::DrawQuad(transform, texture, tilingFactor, tintColor); // Ref<Texture2D>; null = white
Renderer2D::EndScene();
```

- Each view begun in a frame gets its own pass in `Renderer2D::Render`, in `BeginScene` order. A view not begun in a frame isn't rendered (Oxygen skips hidden windows).
- `view.SetSize(w, h)` resizes it from the next render (the editor follows its window sizes). The old images go through the deletion queue, so a resize doesn't wait for the GPU; each frame index's ImGui texture handle is pointed at its new image when that index is recorded next.
- The quad pipeline is built once, from the views' attachment format (`ViewColorFormat`, SRGB; dynamic rendering, no render pass) and a layout-only descriptor set. Every view's target has the same format and its set 0 (camera + textures) is built the same way, so `VulkanPipeline::BindDescriptorSet` binds the view's own set.
- Views are resources: the application creates them after `Renderer::Init` and destroys them while the renderer is alive (Oxygen does it in `OnDetach`).
- `RenderView.h` has no Vulkan types: `GetImGuiTexture()` returns an `ImTextureID`. The view's GPU state (`RenderViewData`: target, batches, texture sets, lights, draws, graph) is in `Platform/Vulkan/VulkanRenderViewData.h`, shared by `RenderView.cpp` and `Renderer2D.cpp`; a view registers itself with `Renderer2D` when made and unregisters when destroyed.
- `RenderViewSpecification::EditorView` marks the editor's scene view (Oxygen's Editor window). It gets the entity-ID attachment ([below](#entity-id-attachment)); the Game view never does.

### Render features and the render graph

Since M1c ([#115](https://github.com/joelnl74/Hydro/issues/115), [plan](https://github.com/joelnl74/Hydro/wiki/Renderer-3D-MVP-Plan)) what a view draws is chosen by **render feature settings**, not by game-specific modes, and recorded through a small fixed-stage **render graph** (v1).

- `RenderFeatureSettings` (`Renderer/RenderFeatures.h`): `Depth` (Off / Shared), `SpriteSorting` (SortOrder / ViewDepth), `Lighting` (None / LightBuffer2D / Forward), `DefaultSpriteAlpha` (Translucent / Cutout), `Meshes`, `Shadows` (Off / Sun), `SpriteShadows` (None / AsDrawn / FaceSun). Presets: `Preset2D()` (today's frame: no depth, sort order, the 2D light buffer, translucent sprites) and `Preset3D()` (shared depth, view-depth sorting, forward lights, cut-out sprites, meshes, sun shadows with sun-facing sprite shadows; settings saved before shadows existed load with `Shadows = Off` and show as Custom); `GetPresetName()` says "2D", "3D" or "Custom".
- The project default is `RendererSettings::Features` (saved in the `.hyproj` under `Renderer: RenderFeatures:`); a scene can override it with `SceneRenderSettings` (`Scene::Get/SetRenderSettings`, saved in the `.hyscene` only when `OverrideProject` is on). `SceneRenderer::GetEffectiveRenderFeatures()` picks one and calls `view.SetRenderFeatures(...)` at the start of `RenderEditor` / `RenderRuntime`. Both are edited in Oxygen's **Render Settings** window (Rendering (project), Rendering (scene)).
- `Renderer2D::CreateRenderFeatures(settings, editorView)` turns the settings into `RenderFeature`s: `EntityID` (Editor view only, M1d, below), `LightBuffer2D` (passes LightBuffer at BeforeScene, LightComposite at AfterLighting), `Depth` (S1, below), `Meshes` (M4: the Meshes pass at Opaque, `Renderer3D::RecordMeshes`), `Sprites` (Decals and CutoutSprites at Opaque with shared depth, Sprites at Translucent), `Overlay` (Overlay), `ForwardLighting` (M6: no pass of its own, it switches the sprite and mesh shaders to their lit variants; see [Forward lighting](#forward-lighting)), `SunShadows` (R14: the SunShadows pass at BeforeScene, see [Sun shadows](#sun-shadows)) and `DebugLines` (Editor view only, M7: the DebugLines pass at Debug).
- Each feature `Validate`s against a `RenderGraphContext` (settings, `RendererCapabilities`, editor view or not), then `Setup`s a `RenderGraphBuilder`: imported resources (`ImportResource`), passes (`AddPass`: name, `RenderStage`, declared `Reads` / `Writes`, an `Execute` callback taking a `RenderPassContext`), and attachment needs (`RequireDepthAttachment`, `RequireEntityIDAttachment`).
- `RenderGraph` (`Renderer/RenderGraph.h`) is built **once per settings change**, not per frame: it drops features with an `Error` message (the frame still draws without them; a `Warning` keeps the feature), stable-sorts passes by stage (BeforeScene, Opaque, Translucent, AfterLighting, Overlay, Debug), checks each read was imported or written by an earlier pass, and logs its messages once. `Renderer2D::ValidateRenderFeatures(settings, editorView)` runs the same validation without building anything (the settings window shows its messages).
- Recording a view: the pass-list rebuild if due (`UpdateRenderGraph`), `PrepareSunShadows`, uploads, then `ExecuteStage(BeforeScene)` outside the target (the light buffer and sun shadow passes begin their own targets), `Target->Begin`, stages Opaque to Debug, `Target->End`, capture.
- Rebuilds: a settings or editor-view change rebuilds the pass list; a change in the depth / entity-ID needs also recreates the view's target. Cached view pipelines are dropped after a shader reload or a wireframe change and built again on first use (pipeline invalidation, see [View pipelines](#view-pipelines)).
- Forward lighting without shared depth is a warning (it still lights, painter's order). No plugin system, automatic scheduling or graph editor in v1.

### Shared depth

Since S1 ([plan](https://github.com/joelnl74/Hydro/wiki/Renderer-3D-MVP-Plan)) `Depth = Shared` gives the view's target a `DEPTH32F` attachment (the `Depth` feature; an error when the GPU can't use that format, then the view draws without depth). It is cleared to 1 once, when the view's pass begins, and kept through every stage, because all of them record into that one rendering pass. Depth compares with `LESS_OR_EQUAL`.

- **What goes where.** `SceneRenderer::SubmitSprites` picks a `SpriteDrawMode` for each item with `Renderer2D::SetSpriteDrawMode`, keeping its sort order. A sprite's `SpriteRendererComponent::AlphaMode` (M3: `Default`, `Cutout`, `Translucent`; saved only when not `Default`) decides: **Cutout** sprites go to the cut-out list; `Default` means `Cutout` for standing sprites (Upright, Billboard) when the view's `DefaultSpriteAlpha = Cutout`, otherwise translucent. Flat sprites that aren't marked `Cutout` are **Decals** when they come before the first standing item (with view-depth sorting: every flat one); particles, translucent sprites and flat sprites layered above standing ones are **Translucent**. Each mode has its own draw list in the view.
- **Order in the frame (Opaque, then Translucent):** meshes (depth test and write, see [Meshes](#meshes)) → decals (depth test, no write, painter's order, a small depth bias towards the camera: constant -4, slope -1, so they sit on ground geometry of the same height) → cut-out sprites (`sprite_cutout.frag` discards alpha under 0.5 and writes the rest opaque, no blending; depth test and write) → translucent quads (depth test, no write, alpha blending, in sort order) → the light composite, if any → overlay quads (no depth test): unlit particles, then HUD sprites, then editor outlines. Decals come before the cut-outs, not after, because in a top-down view standing sprites lie on the ground plane at the same depth as a decal there; drawn later, the decal would pass `LESS_OR_EQUAL` and cover them.
- **Sorting** (`RenderFeatureSettings::SpriteSorting`, SceneRenderer): `SortOrder` (the 2D preset; today's order bit for bit: `SortOrder`, then flat by Z, then standing far to near along the ground) or `ViewDepth` (3D): flat sprites first (by `SortOrder`, then Z), then everything standing back to front by the distance of its pivot (an Upright's feet, a Billboard's or emitter's centre) along the camera's view direction, `SortOrder` only breaking ties. Cut-outs don't depend on the order (depth sorts them); translucent sprites and particles do. `VillageYSort` isn't needed in view-depth scenes; 2D scenes keep it.
- **HUD sprites** (`SpriteRendererComponent::Overlay`, saved only when on): `SubmitOverlaySprites` draws them in the overlay pass after the unlit particles, in `SortOrder`: unlit, never depth tested, in both presets. They write their entity ID in the Editor view. The MVP scene's `UI *` sprites are marked; runtime UI (U1-U7) replaces them later.
- **Pipelines** for views with depth come from the [view pipeline cache](#view-pipelines).
- **Without shared depth** nothing changes: the modes are ignored, every quad goes to the one translucent list in submission order, and the 2D quad pipeline (no depth) draws it.
- `fragment.frag` and `sprite_cutout.frag` share the texture array and `SampleTexture` through `#include "sprite_common.glsl"` (shaderc resolves includes next to the including file; hot reload watches them).
- The S1 spike (a temporary unlit box, checked in the editor on October 25, 2026: occlusion right per pixel in both projections, the road never over the box) was replaced by real meshes in M4; its test area in the MVP scene now has mesh shapes ([Test bed](TestBed.md#features)).
- **Top-down with shared depth:** standing sprites lie flat at z = 0 there, so anything with real height (meshes) covers them. Scenes with meshes are meant to be seen through the tilted camera.

### Meshes

Since M4 static meshes are drawn by `Renderer3D` (`Renderer/Renderer3D.h`), a part of `Renderer` (`Init` / `Shutdown`), into the shared depth buffer before the sprites.

- **`Mesh`** (`Renderer/Mesh.h`): a static mesh from `MeshData` (vertices `MeshVertex`: position, normal, UV, colour; uint32 indices; `SubMesh` ranges with a material slot each), uploaded once to GPU-only buffers (`MeshBuffers`, `Platform/Vulkan/VulkanMeshData.h`), with local bounds. Shared through `Ref<Mesh>`.
- **`MeshFactory`** (`Renderer/MeshFactory.h`): the built-in shapes `Plane`, `Box`, `Ramp`, `Stairs(steps)`, `Cylinder(segments)`. Z-up, metres, base centred on the origin at z = 0, flat-shaded faces counter-clockwise seen from outside (back faces are culled), UVs in metres so a material's tiling is repeats per metre on every face.
- **`MeshComponent`** (`Scene/Components/Components.h`): a built-in `MeshShape` with its `Size` (and `Steps` / `Segments`), one `MaterialData` per sub-mesh, `CastShadows` (kept for H6). Saved, copied on Play, in the inspector (**Mesh** section) and **Add Component**. A mesh asset handle comes with glTF import (M5).
- **The Standard material** (`MaterialData`, `Renderer/Material.h`): albedo and emissive textures (handles; 0 = white), tint, tiling, emission colour and intensity, alpha cut-off (0 = opaque), filter (nearest by default). `mesh.frag`: `albedo * tint + emissive * emission`, pixels below the cut-off discarded; lit by the view's forward lighting when it has it (M6, `lighting.glsl`: wrapped diffuse, emission added after, then fog), else unlit. A slot uses a **material asset** when it names one, else its inline settings (below). Renderer3D keeps a **material cache** keyed by the data: each material has a uniform buffer (`MaterialUniform`, std140, `static_assert`s) and a persistent descriptor set in **set 1** (albedo, emissive, nearest or linear repeating sampler), so meshes with equal materials share one set; a texture that is (re)loaded gives the material a new set (the old one goes through the deletion queue).
- **Material assets** (M4b): a `.hymat` file (`MaterialSerializer`: `Handle` first, then `Material:` with `Type: Standard` and the same fields an inline slot saves) is an `Asset` of type `Material`, loaded by the `AssetManager` together with its textures (`MaterialAsset::LoadUncached`). A `MeshComponent` slot (`MeshMaterialSlot`) holds the asset's handle and its inline settings as the fallback (drawn when the slot has no asset or the file is missing; a picked asset's data is copied there). `SceneRenderer` resolves a slot with `GetLoadedAsset` every frame, so render code never loads; scene loading and the inspector load it. A material asset is plain data and, unlike GPU resources, editable: the inspector changes it in memory with `SetData` (every mesh using it changes that frame; Renderer3D's cache makes the GPU material for the new data) and writes the file once no field is active, so dragging a slider doesn't write every frame. Editing it in Play mode edits the shared asset too. Created with **New Material** (Project window) or **Save as Material Asset** on a mesh slot (into `Assets/Materials`, named after the entity). A file changed outside the editor is read again by the next `GetAsset` (scene load, or the inspector showing it).
- **Models** (M5): a `.gltf` / `.glb` file is a `Mesh` asset (`AssetType::Mesh`, handle in a `.hymeta` sidecar like images). `MeshImporter` (`Renderer/MeshImporter.*`, through cgltf; editor-only until the runtime's cooked format, RT8) reads it into an `ImportedModel` (nodes, meshes with their primitives, materials, images; nothing engine-specific) and `Flatten`s the selected scene (its `scene`, else the first; others are logged) into one `MeshData`: every (node, primitive) is a sub-mesh with the node's world transform baked in, then the import transform (scale, Y-up to Z-up: +90 degrees about X, so the model's front faces -Y, towards the camera). Normals go through the inverse transpose; a mirroring node (negative determinant) has its winding flipped; missing normals are made flat. A mesh used by several nodes is copied per node. Sub-meshes remember their node (`SourceNode`, `SourceName`) and primitive; the `ImportedModel` keeps the node tree for a later "import as hierarchy". Skins, animations and morph targets are logged and ignored, Draco primitives and non-triangle primitives left out.
- **Model materials and textures** (`MeshImporter::ImportAsset`, `Renderer/MeshAssetImport.cpp`): each glTF material becomes a Standard material asset, `<model>_<material>.hymat` next to the model (base colour factor -> tint, base colour texture -> albedo, its `KHR_texture_transform` scale -> tiling, emissive factor and texture, `KHR_materials_emissive_strength` -> intensity, MASK -> alpha cut-off, a LINEAR sampler -> linear filter; BLEND is cut out at 0.5 with a warning; normal, occlusion and metallic-roughness maps are ignored). Embedded images (GLB buffer views, data URIs) are written byte for byte to `<model>.textures/<index>_<name>.<ext>`, the extension from the MIME type (PNG, JPEG, BMP, TGA; KTX2 and WebP are reported and the material keeps its colour); external image files inside the asset folder are registered where they are. The model's `.hymeta` keeps its import settings and the mapping (image index -> texture handle, material index -> material handle) in a `MeshImport` section (`MeshImportSerializer`), so a re-import or a changed file reuses the same assets: an extracted image is rewritten only when its bytes changed, a material is never overwritten (it may have been edited). The AssetManager reads a file's version again after loading it, so an importer writing its own `.hymeta` doesn't count as a change.
- **A model on a MeshComponent:** `MeshComponent::Model` (a Mesh asset handle; 0 = the built-in shape). A slot the component doesn't have uses the model's own material for it (`Mesh::GetMaterials`, the assets its import made), so `Materials: []` means "the model's materials"; the inspector's **Choose materials here** makes a slot per material to change one. Scene loading imports the model (`GetAsset<Mesh>`), so render code finds it with `GetLoadedAsset`. Editor: the Project window shows models as cube tiles (teal strip); clicking one shows its import settings (Scale, Y up to Z up, Make normals; **Apply** re-imports with `ReloadAsset`), sub-meshes and materials in the inspector; dropping one into the Editor window makes an entity with a Mesh component at the camera's focus.
- **Drawing:** `SceneRenderer::SubmitMeshes` (when the effective render features have `Meshes` and shared depth; otherwise a warning once per scene) calls `Renderer3D::BeginScene(view)`, `DrawMesh(mesh, subMesh, material, transform, entityID)` per sub-mesh, `EndScene`. Built-in shapes come from a shape cache (`GetShapeMesh`, keyed by shape and sizes). The Meshes pass sorts the view's draws by pipeline, material and front to back, binds set 0 (the view's camera) and set 1, and pushes `ObjectPushConstants` (transform, entity ID) per draw.
- **Pipelines:** one per attachment layout, entity-ID write and wireframe (Renderer3D's own small cache; cleared when the `Mesh` shader reloads): depth test and write, no blending, back faces culled. Pickable draws write their entity's ID in the Editor view.
- **Lifetime:** shapes and materials no frame has used for 120 frames are freed (`Renderer3D::EndFrame`, after the frame is recorded), so dragging a size or colour slider doesn't pile them up. A new shape size is uploaded with `SubmitImmediate` (it waits for the GPU once).
- **Stats:** the Stats window's **Meshes** section: mesh draw calls, triangles, cached shapes and materials.

### View pipelines

A view's pipelines must match its attachment layout exactly, and views differ: the Game view has one colour attachment, the Editor view adds the entity ID, shared depth adds `DEPTH32F`. `GetViewPipeline(kind, target, writeEntityID)` (in `Renderer2D.cpp`) returns the pipeline for one kind of draw (`Sprite` for translucent quads, `Decal`, `Cutout`, `Overlay`, `Composite`) in one layout; meshes have their own small cache in Renderer3D:

- **The plain 2D layout** (one colour attachment, no depth) uses the pipelines made at `Init` (`QuadPipeline`, `CompositePipeline`), exactly as before, so 2D-preset captures of the Game view can't change.
- **Every other layout** is built on first use by `BuildViewPipeline` from those pipelines' specifications (so shaders, the vertex layout, the descriptor sets and wireframe carry over) and kept in a cache keyed by `ViewPipelineKey`: the kind, the colour formats in order, the depth format, and whether the entity ID is written. Depth test and write follow from the kind: sprites test (no write), cut-outs test and write, overlay and composite do neither.
- **Invalidation:** the pipelines of a shader are dropped after that shader reloads (quad, composite, cut-out) and after a wireframe change; the old pipelines go through the deletion queue. A pipeline that fails to build is logged once and its draws are skipped until the next shader change.

### Buffers and GPU data

Since M2a every GPU buffer is a `VulkanBuffer` (`Platform/Vulkan/VulkanBuffer.h`) made from a `BufferSpecification` (`Renderer/Buffer.h`: `BufferUsage` Vertex / Index / Uniform / Storage / Staging, `BufferMemory` GpuOnly / HostVisible, size, debug name). It replaced `VulkanVertexBuffer`, `VulkanIndexBuffer` and `VulkanUniformBuffer`.

- **HostVisible** buffers are mapped once, for their whole life (VMA `MAPPED_BIT`; `CPU_TO_GPU` memory, `CPU_ONLY` for staging). `SetData(data, size, offset)` copies into the mapping and flushes the range (`vmaFlushAllocation`, nothing to do on coherent memory), so there is no map and unmap per frame ([#62](https://github.com/joelnl74/Hydro/issues/62)). Per-frame data uses `PerFrameBuffers` (one buffer per frame in flight, `CreatePerFrameBuffers`): the views' camera uniforms, the quad batches, the light vertices.
- **GpuOnly** buffers get their contents at creation, through a staging buffer, `vkCmdCopyBuffer` and a `COPY -> ALL_COMMANDS` memory barrier in `VulkanDevice::SubmitImmediate`, which waits: load time only. The 2D quad index buffer and mesh vertices and indices are GPU-only.
- Vertex layouts (`VertexBufferLayout`, `ShaderDataType`) are in `VulkanVertexLayout.h`.
- **GPU data layout** (`Platform/Vulkan/VulkanGPUData.h`): uniform blocks are `std140`, storage buffers and push constants `std430`; fields are vec4-sized (a `vec3` is followed by a `float` in its padding, never another `vec3`); every C++ mirror has `static_assert`s on its size and offsets: `UniformBufferObject` (the camera), `ObjectPushConstants` (transform, entity ID, material index: 80 bytes, for meshes from M4), `QuadVertex` against `GetQuadVertexLayout`, `MeshVertex`, the mesh `MaterialUniform`.
- **Descriptor sets:** set 0 = the frame / view (camera, later lights), set 1 = the material (M4); per-object data in push constants. The 2D quad shader keeps its texture array in set 0 until materials exist.

### Descriptor sets

Since M2b descriptor layouts and sets come from `VulkanDescriptorAllocator` (`Platform/Vulkan/VulkanDescriptors.h`), started and stopped by `Renderer::Init` / `Shutdown`:

- **Layout cache:** `GetLayout(bindings)` makes one `VkDescriptorSetLayout` per distinct list of bindings (binding, type, count, stages) and keeps it until shutdown. Builders with the same bindings share a layout, so their sets and the pipelines built from either are compatible (a view's texture sets and the layout set the quad pipelines are built with).
- **Persistent sets** (`AllocatePersistent` / `FreePersistent`): long-lived sets, a view's texture sets today and materials from M4, from growable pools of 64 sets (`FREE_DESCRIPTOR_SET_BIT`; a full pool is followed by a new one). A set is freed through the deletion queue, after the frames that may still bind it. `VulkanDescriptorBuilder` takes its layout from the cache and its per-frame-in-flight sets from these pools; its API is unchanged.
- **Per-frame sets** (`AllocateFrame`): valid for the frame being recorded only. Each frame in flight has its own pools, reset all at once by `ResetFrame` in `Renderer::BeginFrame`, right after that frame's fence and the deletion queue. Never keep one past its frame, and never use one for a material. The 2D light composite's input is one: `RecordLightPass` makes a set pointing at that frame's light buffer image every frame, so a resized light buffer needs no descriptor rewrite.
- ImGui keeps its own descriptor pool (the backend allocates from it).

### Entity ID attachment

Since M1d the **Editor window's view** has an `R32_SINT` colour attachment after the scene colour (the `EntityID` feature; `RenderTargetSpecification::EntityID`), cleared to -1 when the view's pass begins and left in `SHADER_READ_ONLY_OPTIMAL` by `End`. M7 reads a pixel of it back for click-to-select ([Picking, debug lines and pass timings](#picking-debug-lines-and-pass-timings)).

- **What writes an ID:** every quad has an `int EntityID` (`QuadVertex`, a `ShaderDataType::Int` attribute, `-1` = none). `SceneRenderer` calls `Renderer2D::SetEntityID(entity)` before each entity's sprite and `-1` for particle emitters and after the sprites, so lines, lights, shadows, unlit particles and the overlay carry -1.
- **Shaders:** `vertex.vert` passes it on as a `flat int`; `fragment.frag` and `sprite_cutout.frag` write it to `layout(location = 1) out int`. The translucent shader discards pixels with alpha exactly 0 (they add nothing under alpha blending, so the colour is unchanged), so clicks pass through a sprite's empty parts; the cut-out shader discards under 0.5 before anything is written. Views without the attachment ignore the output.
- **Pickable or not:** a draw is `Pickable` when its quads have an entity ID (`SetEntityID` starts a new draw when that changes). With the ID attachment, pickable draws use a pipeline that writes it; the others (particles, lines) use the same pipeline with the ID attachment's write mask at 0 (`PipelineSpecification::WriteIntegerAttachments = false`), so they don't hide what is under them. The overlay and the light composite never write it; pickable meshes do. The ID attachment is never blended.
- An ID is the entity's `entt::entity` value as an `int32`; one with its top bit set (an entity recycled more than 2047 times) reads as -1 and isn't pickable. There is no per-entity "not pickable" switch yet.

### Picking, debug lines and pass timings

Plan M7 (Oct 26, 2026): the editor's diagnostics on top of the render graph.

- **Click-to-select.** `RenderView::RequestEntityIDReadback(x, y)` (view pixels, top-left origin) asks for the entity-ID pixel under a click. After the view's pass `Renderer2D::RecordPick` copies that one pixel into the frame in flight's `VulkanImageReadback` (the attachment is in `SHADER_READ_ONLY_OPTIMAL`, as for captures); when the same frame index comes round again its fence has signalled and `ResolvePicks` reads it, so nothing waits. `TakeEntityIDResult(id)` is true once per answer (-1 = nothing). Each request and the view's generation are stamped on the slot: a newer request replaces an older one, and a resize (`ApplyViewResize`) or `InvalidateEntityIDRequests` (Oxygen calls it whenever the scene drawn changes: open, new, Play, Stop, scene push/pop) throws earlier answers away. Oxygen (`EditorLayer::UpdateClickSelection`): a left press and release in the Editor image without moving (under 4 px), not with Alt held (camera orbit) or during an asset drag, selects the entity drawn there through the hierarchy, or clears the selection over nothing. Works in both projections; translucent pixels with alpha 0 and non-pickable draws (particles, lines, overlay) pass the click through.
- **3D debug lines.** `Renderer2D::BeginDebugLines` / `DrawLine3D(start, end, colour, thickness)` / `EndDebugLines`: each line is a camera-facing ribbon (its width in pixels, measured at the middle through the inverse view-projection), drawn by the `DebugLines` pass at `RenderStage::Debug` with the depth test when the view has shared depth (no depth write, no entity ID, unlit). Only the Editor view has the feature. `SceneRenderer::SubmitSelectionDebugLines` draws the selection's mesh bounds (12 edges), a point light's three range circles and its Height stalk, a spot light's cone, and the sun's direction arrow. Counted in the Stats window (Lighting: Debug lines).
- **Pass timings and labels.** `Renderer2D::ExecuteStage` wraps every render-graph pass in a `VK_EXT_debug_utils` label ("View <name>" around each view, the pass name inside, so RenderDoc's event browser follows the graph) and two timestamps (up to `MaxTimedPasses` per frame). `GetPassTimings()` sums them by pass name over the views, a couple of frames old; the Stats window shows them under **Passes**. `VulkanRendererContext` now enables `VK_EXT_debug_utils` whenever the loader has it, not only with validation; labels are skipped when it is missing.

### Captures

`view.RequestCapture(path)` saves the next picture the view renders as a PNG (the comparison tool for renderer changes: step 0 of the [3D renderer MVP plan](https://github.com/joelnl74/Hydro/wiki/Renderer-3D-MVP-Plan), [#113](https://github.com/joelnl74/Hydro/issues/113)).

- Right after the view's pass, `Renderer2D` records a copy of its target image into a host-visible buffer for that frame in flight (`VulkanImageReadback`: barrier to `TRANSFER_SRC`, `vkCmdCopyImageToBuffer`, barrier back to `SHADER_READ_ONLY_OPTIMAL`, so ImGui still samples it later in the command buffer). Nothing waits: the next time the same frame index is rendered, its fence has signalled, and `Renderer2D::Render` takes the pixels and writes the file with `ImageWriter::WritePNG` (stb_image_write). So the file appears `MAX_FRAMES_IN_FLIGHT` frames after the capture.
- The target is sRGB (`R8G8B8A8_SRGB`), so the bytes go into the PNG as they are; alpha is written opaque. The overlay (editor outlines) is included.
- A view that isn't rendered (hidden window, minimised) keeps the request until it is. `IsCapturePending()` is true until the file is written. The PNG is encoded on the main thread: a short hitch for a large view.
- `VulkanImageReadback` copies any region of a colour image (RGBA8, BGRA8, R32, RGBA16F/32F); click-to-select reads the entity-ID attachment with it (M7). Colour attachments are made with `TRANSFER_SRC` usage for this.
- Oxygen: the **Capture** menu in the Editor and Game windows, **F9** (Game view) and **Shift+F9** (Editor view; not F12, which Windows reserves for debuggers and which breaks into Visual Studio while debugging). **Fixed Size (1280 x 720)** (on by default) renders the captured frame at that size, with the scene's or editor camera's viewport set to it for that frame, so captures don't depend on the window layout; the window shows that one frame stretched. Files go to `<project>/.hydro/captures/<scene>_<view>_<yyyymmdd-hhmmss-mmm>.png` and the Console logs each one.
- `scripts/compare_captures.py a.png b.png [--out diff.png] [--tolerance N]` counts differing pixels, reports the largest difference and writes a diff image (Pillow). Baselines for the 3D work are listed on [Test bed](TestBed.md#baseline-captures).

### Textures and batching

`Texture2D` (`Renderer/Texture2D.h`) is the engine-facing texture: RGBA8, made from an `ImageSpecification` (size, `ImageFormat::SRGB` for colour or `RGBA` for data) and `TextureProperties` (filter, wrap), on a `VulkanImage` (VMA, one upload). `Texture2D::LoadFromFile(path)` returns `nullptr` (logged) on failure; a file in the project's asset folder is loaded and shared by the `AssetManager` ([Asset system](#asset-system)), any other file is decoded by `TextureImporter` (stb_image) and shared while a texture from it is alive. Textures are shared by many sprites, so they are held in `Ref<Texture2D>`. The header has no Vulkan types: the image lives in `Texture2DData` (`Platform/Vulkan/VulkanTexture2DData.h`).

A view's quads are recorded as a list of **draws** (one `vkCmdDrawIndexed` each), in submission order:

- **Batches** hold vertices: up to 20,000 quads per vertex buffer (one per frame in flight). When one is full the next is used, allocated the first time a frame needs it.
- **Texture sets** hold up to 32 textures (`Renderer2D::GetMaxTextureSlots()`; slot 0 is a 1×1 white texture made in code, so untextured quads need no file) and set 0's descriptor sets (camera + texture array, one per frame in flight). `DrawQuad` looks the texture up in the current set and adds it to a free slot.
- A new draw starts when the batch is full or when a quad needs a 33rd texture (a **texture flush**: the next set is used, with fresh slots). A flush costs one more draw call, not a new vertex buffer, and earlier draws are recorded first, so draw order is kept. There is no limit on quads or textures per frame. Batches and sets are reused every frame; nothing is allocated once a scene has been drawn.
- `RecordView` writes each used set's texture array into that frame's descriptor set before the pass (the frame's fence has signalled, so the GPU is done with it), uploads the batches, then binds vertex buffer and descriptor set only when they change between draws.
- **Lifetime.** A texture set holds `Ref`s to its textures only while the frame is being built. When the last `Ref` goes, the texture's `VulkanImage` hands its handles to the deletion queue, which frees them after the frames in flight that may sample it. So dropping a texture mid-frame (changing a sprite's texture, deleting an entity) is safe without `Renderer::WaitIdle`.
- The quad shader picks the texture with a `switch` over constant indices (`textureGrad` with derivatives taken before it), because indexing the sampler array with a per-quad value needs descriptor indexing, which the device doesn't enable. `texIndex` is `flat`. The vertex colour multiplies the texture, alpha included.
- Texture coordinates run `v = 1` at the quad's bottom to `v = 0` at its top (Vulkan's first image row is the top), times the tiling factor; samplers repeat by default.
- **Stats** (Oxygen's Stats window): slots per draw, slots used, texture flushes.
- **Particles** (Oct 25, 2026): `SceneRenderer` puts each lit `ParticleEmitter2DComponent` with live particles into the sprite sort (its `SortOrder`, then Z) and draws its particles together with `Renderer2D::DrawQuad(position, size, rotation, texture, rect, colour)`, a fast path that writes the four corners without a matrix per quad. Colour and size are interpolated from each particle's age. The default particle texture is a soft round dot made in code (`Renderer2D::GetSoftDotTexture()`, 32 x 32, clamped). **Unlit** emitters are drawn between `BeginOverlay` / `EndOverlay`, after the view's lighting is composited, so they stay bright at night but go on top of every sprite (the first candidate for the emissive path; the Roadmap V4 spike decides). `ParticleBlend::Additive` still draws alpha-blended until the premultiplied quad pipeline (V4).

### 2D lighting

A view that is given at least one light between `BeginScene` and `EndScene` is drawn lit (when `RendererSettings::Lighting` is on; views without lights are drawn exactly as before). `SceneRenderer` submits every `Light2DComponent`: `Renderer2D::DrawLight(position, radius, color, intensity)` for a point light, `DrawGlobalLight(color, intensity)` for a global one.

1. **Light buffer pass** (before the view's scene pass, `RecordLightPass`). An RGBA16F target the size of the view, made the first time the view is lit and resized with it. It is cleared to the sum of the global lights (the ambient colour, 0 when there are none); each point light is then added with one quad covering its radius (`LightPipeline`, `BlendMode::Additive`, `assets/shaders/light.frag`, one indexed draw for all lights).
   **Blob shadows** are drawn into the same buffer right after the lights: `SceneRenderer` submits every `Shadow2DComponent` with `Renderer2D::DrawShadow(position, size, strength)`, each an ellipse quad (`ShadowPipeline`, `BlendMode::Darken`: `dst * (1 - alpha)`, `assets/shaders/shadow.frag`, one indexed draw, vertices after the lights' in the same buffer). So a shadow darkens whatever light reaches that spot, ambient included, before the composite multiplies the buffer over the sprites. It is a grounding blob, not a cast shadow: it does not depend on where the lights are. `RendererSettings::Shadows` turns it off.
2. **Composite** (last draw of the scene pass, `RecordComposite`). A fullscreen triangle (`composite.vert`, no vertex buffer) samples the light buffer and multiplies it over everything drawn so far (`BlendMode::Multiply2x`: `dst * src * 2`, so a light of 1 leaves a colour as it is and up to 2 over-brightens it). Blending happens in linear space.

`RendererSettings::LightFalloff` picks a smooth fade or `LightBands` visible steps; it reaches `light.frag` in the light quads' vertices (the "texture index": 0 = smooth, otherwise the band count). The curve itself is in the shader file, and light shaders are hot reloaded like the quad shader. Lighting is optional: if its shaders can't be built, `Renderer2D::Init` logs it and every view is drawn unlit. Shadows are optional on top of that (`shadow.frag` missing or broken: lit views are drawn without them).

### Forward lighting

Plan M6 (Oct 26, 2026): `RenderFeatureSettings::Lighting = Forward` (the 3D preset) lights sprites and meshes in their own shaders instead of through the 2D light buffer.

- **Gathering** (`SceneRenderer::SubmitForwardLighting`, called from `SubmitLights` in forward views): a `SceneLighting` (`Renderer/Lighting.h`, no Vulkan) with the camera position; the **sun** (`Scene::GetMainLight()`: the `DirectionalLightComponent` marked `IsMainLight`, else the first in the scene's order, a warning once when several are marked), shining along its entity's -Z turned by its rotation, colour times intensity; **ambient and fog** from the scene's `SceneEnvironment` (`Scene::Get/SetEnvironment`, saved as `Environment:` in the `.hyscene` when `Override` is on, edited in Render Settings > Environment (scene)) or, without one, the sum of the scene's Global lights for sky and ground alike and no fog; every **point** light at its translation plus `Height` along Z, and every **spot** light (`Light2DType::Spot`, `InnerAngle` / `OuterAngle` in degrees, pointing along its -Z). `Renderer2D::SetSceneLighting` stores it on the view.
- **Upload** (`Renderer2D::UploadLighting`, per view and frame in flight, before recording): `FrameLightingUniform` (std140, 144 bytes) at set 0 binding 2 and up to `MaxForwardLightsPerView` (64) `GPULight`s (std430, 64 bytes) in a storage buffer at binding 3 (`VulkanGPUData.h`, `static_assert`s). More lights keep the 64 nearest the camera (a warning once; Stats: Lights dropped). `Counts.y = 1` marks the view forward lit; a view that isn't (or whose scene submitted nothing) uploads zeros and the shaders draw unlit.
- **Shaders** (`assets/shaders/lighting.glsl`, included): hemisphere ambient (ground to sky by the normal's Z), a wrapped sun diffuse, the point and spot lights with the same falloff as the light buffer (`light_falloff.glsl`, shared with `light.frag`, banded by `RendererSettings::LightFalloff` / `LightBands`) and a smoothstep cone, then linear distance fog. Fog distance is measured from `SceneLighting::FogOrigin`, where the camera's view meets the ground (z = 0; the camera itself when it looks level or up), not from the camera: a top-down editor camera high above the scene sees it as clearly as the tilted game camera, and in play the haze stays around the player. Sprites use `fragment_lit.frag` / `sprite_cutout_lit.frag` (`vertex.vert` now passes the world position): the normal comes from the position's derivatives turned towards the camera, with a strong wrap so a sprite lit from behind is dim, not black. Meshes (`mesh.frag`) use the vertex normal (flipped on back faces) with a light wrap; emission is added after lighting. The pipeline cache key has a `Lit` flag; overlay and debug-line draws are never lit.
- **Blob shadows** in forward views are soft dark decals (`Renderer2D::GetSoftDotTexture`, `SpriteDrawMode::Decal`, 1 cm above the ground) instead of `DrawShadow` into a light buffer.
- Stats window, Lighting: forward-lit views, forward lights, lights dropped.

### Sun shadows

Roadmap R14 / Diorama RPG H6 (Oct 26, 2026): `RenderFeatureSettings::Shadows = Sun` (in the 3D preset) gives the main `DirectionalLightComponent` one shadow map. Needs `Lighting = Forward` and `Depth = Shared` (validation errors otherwise: the feature is left out, the frame still draws).

- **The map** (`Renderer2D::PrepareSunShadows`, per view before the uploads): active this frame when the graph has the `SunShadows` pass (`RenderGraph::HasPass`) and the scene's sun has `CastShadows`. A `DEPTH32F` `VulkanRenderTarget` per view (`RenderViewData::ShadowTarget`, no colour attachments, `RenderTargetSpecification::SampledDepth`: `End` moves it to `SHADER_READ_ONLY_OPTIMAL`), `RendererSettings::SunShadowMapSize` square (1024 / 2048 / 4096, resized when the setting changes). One orthographic camera (`Camera::Orthographic`) looks along the sunlight at `SceneLighting::FogOrigin` (what the camera looks at), covering the light's `ShadowDistance` metres each way and starting 60 m further towards the sun so casters outside the area still shadow into it; the projection is snapped to whole texels so shadow edges don't crawl when the camera moves. No cascades: a diorama's visible area is small.
- **The pass** (`SunShadows`, `RenderStage::BeforeScene`, `Renderer2D::RecordSunShadowPass`): clears the map, draws the meshes whose `MeshComponent::CastShadows` is on (`Renderer3D::RecordShadowCasters`: depth only, both faces, alpha cut like their material, `mesh_shadow.vert/.frag`, the `MeshShadowCaster` shader), then the sprite casters (`ViewPipelineKind::ShadowCaster`, `shadow_caster.vert/.frag`, alpha tested at 0.5), both with a rasteriser depth bias (`ShadowCasterDepthBias*` in `VulkanRenderViewData.h`). Synchronisation is the one hand-written transition at `End`, as for the 2D light buffer (render graph v1).
- **Sprite casters** (`SceneRenderer::PrepareSunShadowCasting`, `SubmitSprite`): cut-out Upright and Billboard sprites with `SpriteRendererComponent::CastShadows` cast, by `RenderFeatureSettings::SpriteShadows`: `None`; `AsDrawn` (the camera-facing quad: cheap, thin when the sun comes from the side); `FaceSun` (Octopath style: a copy of the quad stood straight up on the sprite's feet and turned about Z to face the sun, the way round closer to the camera's so the sprite's left casts on the left). The casters are queued and submitted after the sprites in one `Renderer2D::BeginShadowCasters` / `EndShadowCasters` block (their own draw list, drawn only into the map). Translucent sprites and particles don't cast.
- **Receiving** (`lighting.glsl`): set 0 binding 4 is a `sampler2DShadow` (a linear comparison sampler: 2 x 2 PCF per tap), always bound (an empty 1 x 1 map in views without one this frame); `FrameLightingUniform` (now 224 bytes, declared in `frame_lighting.glsl`) carries `ShadowMatrix`, `ShadowParams` (strength, texel size, soft or hard, normal offset) and `Counts.z` (map present). `SunShadow` darkens only the sun's diffuse (ambient, point and spot lights reach into shadow), soft (3 x 3 taps, `RendererSettings::SoftSunShadows`) or hard (the texel's own result: crisp steps for pixel art), lit outside the map. Meshes look up at their position pushed out along the normal (`SunShadowSurface`). Sprites look up at `QuadVertex::ShadowPosition` (`vertex.vert` location 5): with FaceSun that is the same texel of their own caster quad (`Renderer2D::DrawQuad(..., shadowLookupTransform)`), pushed towards the sun, so a sprite is never darkened by its own shadow, only by what stands between it and the sun.
- **Per light**: `DirectionalLightComponent::CastShadows`, `ShadowStrength` (0..1), `ShadowDistance` (metres), `ShadowBias` (scales the normal offset). Stats window: sun shadow maps and sprite shadow casters (Lighting), mesh shadow draws (Meshes); the pass's GPU time is under Passes.

### Sprite sheets and import settings

A sprite sheet is not a separate type: like Unity, every `Texture2D` has a **sprite mode**. `Single` (the default) is one sprite, the whole image; `Multiple` is a sheet sliced into named sprites. How an image is imported is a `TextureImportConfig` (`Renderer/TextureImport.h`), saved next to it as `<image>.hymeta` (YAML, `TextureImportSerializer`): format, filter, wrap, an optional colour key, sprite mode, pixels per unit, the slicing settings and the sprite list.

- `TextureImporter::Import` (used by `LoadFromFile` and the `AssetManager`) reads the `.hymeta` when there is one (its settings replace the caller's `properties`/`format`). A `Multiple` config with slicing settings but no `Sprites` is sliced on load (`SpriteSlicer::SliceGrid`; `Mode: Auto` runs `SpriteSlicer::DetectGrid` on the pixels first), so a hand-written `.hymeta` needs only the grid. The colour key turns that exact colour transparent before upload. The `.hymeta`'s modification time is part of the sharing key (and of a cached asset's version), so saving it and loading again re-imports.
- A texture always has at least one sprite (`GetSpriteCount() >= 1`); sprites outside the image are dropped with a warning. `GetSpriteUVRect(i)` returns the sprite's `TextureRect`, inset by 1/1000 texel so nearest sampling at the quad's edge never picks the neighbour; a whole-image sprite is exactly 0..1.
- `Renderer2D::DrawQuad(transform, texture, TextureRect, tint)` draws part of a texture (swap `Min`/`Max` to flip). The tiling overload is the same path with the rect `(0,0)-(tiling,tiling)`; no shader or vertex change.
- `SpriteRendererComponent::SpriteIndex` picks the sprite (0 by default: the first frame of a sheet). `SceneRenderer` draws a `Multiple` texture's sprite with its UV rect and moves the quad so the sprite's **pivot** sits on the entity (a feet pivot `(0.5, 0)` keeps characters standing still while frames change); `FlipX`/`FlipY` swap the rect.
- Sprite animation (named clips over a sheet, `SpriteAnimatorComponent`, `SpriteAnimator::Play`) is in [Scene, ECS and systems](SceneAndECS.md#sprite-animation).

Scenes are drawn through a `CameraComponent` at runtime or the `EditorCamera` in the editor; see [Scene, ECS and systems](SceneAndECS.md#cameras).

## The editor (Oxygen)

`EditorLayer` owns the editor scene, the play copy, two `RenderView`s, the `EditorCamera` and the editor windows. Windows are `<Name>Window` classes in `Oxygen/src/EditorWindows`, listed under **View**.

| Window | What it does |
| --- | --- |
| **Editor** | The active scene through the editor camera (Camera menu: 2D/orthographic, reset). Mouse input reaches the camera only while it is hovered |
| **Game** | The active scene through its primary `CameraComponent`; its size is the scene's viewport |
| Top bar | The Hydro droplet, the menus and icon buttons for **Play / Stop**, **Pause / Resume**, **Step** (see [Play mode](SceneAndECS.md#play-mode)). A thin accent "waterline" runs under it |
| **Hierarchy** | Entities with a Tag and ID; create, delete, save as prefab, drop prefabs |
| **Inspector** | The selected entity's components, edited with typed input fields; Add Component (including registered scripts). With no entity selected, the asset picked in the Project window: an image's import settings (`TextureImportPanel`: sprite mode, pixels per unit, filter, wrap, colour space, colour key; **Apply** writes the `.hymeta` and re-imports) or a `.hyanim`'s clips, or a material's fields (`.hymat`, saved when an edit is finished) |
| **Project** | The project's asset folder; open scenes, drag prefabs into the scene; image thumbnails (sheets get a strip); click an image or `.hyanim` to inspect it, double-click to open it in the Sprite Editor / Animation window; right-click a sheet > **Create Sprite Animation** (one clip per sprite group); right-click the folder > **New Material** (round swatch tiles with an orange strip; click one to edit it in the inspector). `.hymeta` files are hidden and follow their image on rename and delete |
| **Sprite Editor** | Slices an image into sprites: Grid / Auto (**Detect Grid**) / Manual, row names (sprites are named `<row>_<column>`), per-sprite name, rectangle and pivot (presets), sprites and the selected pivot drawn over the image, Ctrl+wheel zoom. **Apply** sets sprite mode Multiple, writes the `.hymeta` and re-imports |
| **Animation** | Edits a `.hyanim`: sheet (pick or drop), default clip, clips (add, copy, delete, **Clips from Sprite Names**), per clip name / loop / flip / frames (sprite, seconds, order) / events, a live preview and a sprite picker (click to add a frame). **Save** writes the file and reloads the animators that play it |
| **Console** | The `Log` history (engine and client), with level toggles, a text filter and auto-scroll; time and source columns follow Editor Settings |
| **Stats**, **Render Settings**, **Project Settings** | Frame timings and renderer stats; live `RendererSettings`; project name and start scene |
| **Editor Settings** | Pages: **Appearance** (theme, accent colour, text scale, font size), **Play Mode** (tint), **Layout** (rounding, borders, spacing), **Colors** (every ImGui colour), **Editor Camera** (start in 2D, field of view, clip planes, orbit/pan/zoom speed), **Console**, **Startup** (reopen last project and scene, autosave). Applied live; **Save** writes `EditorSettings.yaml` next to `imgui.ini` (per user, not per project); **Revert** reloads it; **Defaults** resets everything |

Only visible windows are rendered: a hidden Editor or Game tab doesn't begin its view that frame.

The editor's look is editor-only: `EditorStyle`, `EditorSettings` (with `EditorSettingsSerializer`) and `EditorChrome` live in Oxygen, and the engine never reads them. The engine's `ImGuiLayer` sets ImGui's dark defaults; `EditorLayer::OnAttach` loads `EditorSettings.yaml` and applies the saved style on top. Missing keys keep the theme's values, so the file still loads after a setting is added. The file has a `Version`; a file from an older version keeps its theme and font scale, and its colours and sizes are rebuilt from the theme.

- **Themes.** **Hydro** (deep ocean blues, the default), **Abyss** (darker) and **Glacier** (light) are built from one accent colour (presets Aqua, Lagoon, Tide, Orchid, Coral, Amber, or any colour): selection, checkmarks, sliders, tabs, focus and the waterline all follow it. **Dark**, **Light** and **Classic** are ImGui's presets.
- **Chrome.** `EditorChrome` draws what a theme can't: the droplet and name at the start of the menu bar, the waterline, the play-control icons and the play-mode tint. While playing, windows, title bars, tabs and the menu bar blend towards the tint colour, the waterline takes the tint colour and the Game window gets an outline.
- **Fonts.** Oxygen loads Open Sans (`Oxygen/assets/Fonts/opensans`, Regular and Bold) through `ApplicationSpecification::ImGui` (an `ImGuiLayerSpecification`). The engine loads only the fonts the application lists; `ImGuiLayer::GetFont("Bold")` finds one by name. The font atlas is built once at startup, so **Font size** applies after a restart (it is read in `CreateApplication`, before the log exists); **Scale** applies at once.
- **Editor camera.** `EditorCameraSettings` (engine, `Renderer/EditorCamera.h`) are live options passed in with `EditorCamera::SetSettings` every frame; changing the field of view keeps the framing at the focal point. `EditorCamera::Update(deltaTime, acceptInput)` takes `OnUpdate`'s delta time for fly mode; the 2D / 3D switch, the standard views (`EditorCameraView`, `SetView`) and the controls are in [Scene, ECS and systems](SceneAndECS.md#cameras).
- **Asset editing.** The windows edit copies and write files; `EditorLayer` then swaps the result into the editor scene and the play copy: a re-imported texture replaces the old one on every sprite that used the file (keeping each sprite by name), animation sets that use it are read again (`SpriteAnimationSet::LoadFromFile(path, true)`), and a saved `.hyanim` replaces the set on its animators (the playing clip carries on by name). Images in these windows go through `Renderer2D::GetImGuiTexture`: a small pool of ImGui handles, each rewritten for another texture only after it hasn't been drawn for longer than the frames in flight, so ImGui's descriptor pool doesn't grow.
- **Session.** The last project and scene, and which View menu windows are open (`EditorSession::OpenWindows`, by menu name), are remembered in the same file. On close, and as soon as a window is opened or closed, `EditorSettingsSerializer::SaveSession` rewrites only that part, so unsaved edits in the window aren't written by accident. `EditorLayer::GetViewWindows()` is the one list behind the View menu, the save and the restore: a new window added there is remembered too; a window missing from the file keeps its own default.

## Device requirements

The engine needs **Vulkan 1.3** (decided October 25, 2026, for the [3D renderer MVP plan](https://github.com/joelnl74/Hydro/wiki/Renderer-3D-MVP-Plan), step M0, [#114](https://github.com/joelnl74/Hydro/issues/114)). Windows only for now; any desktop GPU with a driver from 2022 or later has it. The Vulkan SDK the setup scripts install is 1.3.296.

- `VulkanRendererContext` creates the instance with `HY_REQUIRED_VULKAN_VERSION` (1.3).
- `VulkanPhysicalDevice` reads each GPU's features with `vkGetPhysicalDeviceFeatures2` (the 1.0 features, `VkPhysicalDeviceVulkan12Features`, and `VkPhysicalDeviceVulkan13Features` when the GPU has 1.3) and only picks a GPU with Vulkan 1.3, `dynamicRendering` and `synchronization2` that can present to the window. Every GPU that can't be used is logged with the reason; when none can, startup stops with a message that names them.
- `VulkanDevice` enables one feature chain: `VkPhysicalDeviceFeatures2` in `VkDeviceCreateInfo::pNext` (`pEnabledFeatures` is null) → `VkPhysicalDeviceVulkan13Features`. Required: `dynamicRendering`, `synchronization2`. Optional, enabled when the GPU has them: `samplerAnisotropy`, `fillModeNonSolid` (wireframe), `wideLines`, `independentBlend` (a blend state per colour attachment: the Editor view's entity ID next to the blended scene colour; without it the `EntityID` feature reports an error and the Editor view has no IDs). `descriptorIndexing` is reported but not enabled until bindless textures use it. Code asks the device what was **enabled** (`IsWideLinesEnabled()`), never what the GPU merely supports.
- `RendererCapabilities` (`Renderer::GetCapabilities()`) has the GPU's name and Vulkan version, the required and optional features, and the limits the 3D work depends on: push constant size, sampled images per stage, maximum anisotropy, and whether `DEPTH32F` / `DEPTH24STENCIL8` can be depth attachments. They are logged at startup (four `Renderer:` lines) and listed in **Render Settings > Device**.
- Two places stay on 1.2 on purpose: VMA's `vulkanApiVersion` (the vendored VMA asserts on anything above 1.2; it only decides which memory functions it loads) and shaderc's target environment (SPIR-V for 1.2 runs on a 1.3 device).
- Offscreen passes (every view's scene pass and its 2D light buffer) use **dynamic rendering** since M1 ([#115](https://github.com/joelnl74/Hydro/issues/115)): `VulkanRenderTarget` begins them with `vkCmdBeginRendering` and moves its images between layouts with `vkCmdPipelineBarrier2` (synchronization2), and pipelines are built from attachment formats (`PipelineSpecification::ColorFormats` / `DepthFormat`). The present pass keeps its `VkRenderPass` until ImGui is upgraded: the vendored ImGui 1.89 backend takes a render pass in `ImGui_ImplVulkan_Init`. Mixing the two is valid Vulkan.

### Render targets

`VulkanRenderTarget` (`Platform/Vulkan`, from a `RenderTargetSpecification` in `Renderer/RenderTarget.h`) is what views and the light buffer draw into: colour images in attachment order, an optional depth image and an optional `R32I` entity-ID image, one set per frame in flight.

- `Begin(cmd, frame, RenderTargetBeginInfo)` transitions every attachment to its attachment layout and begins rendering; the info names each attachment's load op (`Clear`, `Load`, `DontCare`) and clear value, nothing is implied. `End(cmd, frame)` ends rendering and moves the colour attachments (entity ID included) to `SHADER_READ_ONLY_OPTIMAL`, which every reader expects: ImGui, the light composite, captures. Depth stays in `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` (not `DEPTH_ATTACHMENT_OPTIMAL`, which would need the `separateDepthStencilLayouts` feature).
- Layouts are tracked per image, so the barriers always start from the real layout (`UNDEFINED` for new images).
- `Resize` makes new images and hands the old ones to the deletion queue, like every resource; it doesn't wait for the GPU. A size of 0 isn't allowed (callers skip minimised views).
- `GetPipelineColorFormats()` / `GetPipelineDepthFormat()` give the attachment layout a pipeline must be built for (the entity ID is the last colour format when there is one).
- Push constants: `PipelineSpecification::PushConstantSize` (vertex and fragment stages) and `VulkanPipeline::PushConstants`. Not used by the 2D pipelines; meshes use them for the object (`ObjectPushConstants`: transform, entity ID, material index).

## Vulkan ownership

| Object | Owns |
| --- | --- |
| `VulkanRendererContext` | Instance, debug messenger (Debug builds), window surface, device (`Scope<VulkanDevice>`, handed out as `VulkanDevice&`). All members: nothing in it is `static` |
| `VulkanDevice` | The GPU it was created for (`Scope<VulkanPhysicalDevice>`, whose properties and features are read once), the enabled features (`Is...Enabled`), logical device, graphics + present queues, the upload command pool (private: only `SubmitImmediate` uses it) |
| `VulkanSwapChain` | Swapchain images, views, framebuffers, present render pass (kept for ImGui), render-finished semaphores; acquire/present; `Recreate` |
| `VulkanFrameContext` | Per frame in flight (`MAX_FRAMES_IN_FLIGHT = 2`): command pool + buffer, fence, image-acquired semaphore |
| `Renderer` | The above (`Scope`s; `GetVulkanDevice()`, `GetRendererContext()` and `GetVulkanSwapChain()` return references), the **deletion queue**, and runs the frame |
| `VulkanRenderTarget` | An offscreen target for dynamic rendering: colour, optional depth and entity-ID images per frame in flight, their tracked layouts. Each `RenderView` owns one, and a lit view a second for its light buffer |
| `RenderView` | Its target, a uniform buffer for its camera, its quad batches (per-frame vertex buffers) and texture sets (per-frame descriptor sets: camera + 32 textures), and, after its first capture, a `VulkanImageReadback` (one host buffer per frame in flight), all in `Scope`s |
| `Renderer2D` | The quad shader, pipeline, index buffer (GPU-only), the layout descriptor set the pipelines are built with, the view pipeline cache, and the white texture |
| `Texture2D` | One `VulkanImage` (image, view, sampler; VMA). Shared through `Ref<Texture2D>` |

GPU resources (`VulkanImage`, `VulkanRenderTarget`, `VulkanBuffer`, `VulkanDescriptorBuilder`, `VulkanPipeline`, `VulkanShader`) free their handles in their destructor and are non-copyable; their single owner holds them in a `Scope<T>` (see [Resource creation](ResourceCreation.md#ownership)). A destructor doesn't destroy the handles itself: it hands a function that does to the **deletion queue**, `Renderer::SubmitResourceFree(std::function<void()>)`. The renderer tags it with the current frame number and runs it at the start of the frame `MAX_FRAMES_IN_FLIGHT` frames later, once that frame slot's fence says the GPU is done with every frame that could have used the resource. It also runs everything queued after a swapchain rebuild (the device is idle then) and at `Renderer::Shutdown`, before the allocator and device go. Frees run in the order they were queued. `VulkanShader` is the exception: shader modules aren't used after a pipeline is created, so it frees them at once.

Rules that follow from this:

- **Anything the CPU writes per frame exists once per frame in flight** (vertex buffers, uniform buffers, offscreen images). Index it with `Renderer::GetRenderFrame()`.
- **Record into the frame's command buffer** (`Renderer::GetCurrentCommandBuffer()` or the one passed to you). Don't create a separate submit for per-frame work.
- **Window resize rebuilds only the swapchain.** Render views follow the windows that show them (`RenderView::SetSize`; the Game window also calls `Scene::OnViewportResize` for camera aspect ratios).
- **Reaching the device.** Backend code (`Platform/Vulkan`, `Renderer2D`, `ImGuiLayer`) calls `Renderer::GetVulkanDevice()`; `Renderer::GetRendererContext()` is only for the instance and surface. Take one `VulkanDevice& device = Renderer::GetVulkanDevice();` at the top of a function rather than repeating the chain. Parts that are started with a device take it as a parameter instead (`VulkanSwapChain`, `VulkanFrameContext::Init(device)`, `VulkanAllocator::Init(instance, device)`). GPU limits and names come from `device.GetPhysicalDevice().GetProperties()` / `GetFeatures()`, not from calling `vkGetPhysicalDeviceProperties` again.
- **Tell the device, don't take its parts.** Work that needs the device's internals is a `VulkanDevice` function, so callers don't fetch pools and queues to do it themselves. **One-off uploads** (texture pixels, the ImGui font atlas) go through `VulkanDevice::SubmitImmediate([&](VkCommandBuffer cmd) { ... })`: it records your commands into a one-time command buffer from the upload pool, submits it and waits on a fence of its own, so staging memory can be freed right after. Frames in flight keep running (no queue-wide wait), but the CPU blocks, so it is for loading, never for per-frame work.
- **Never wait for the GPU to release something.** Drop the `Scope` (or the last `Ref`) and the deletion queue does the rest. `Renderer::WaitIdle()` is only for shutdown and swapchain rebuilds.

## Shaders

Shaders live as GLSL in the application's `assets/shaders` folder and are referenced by their source path (`assets/shaders/vertex.vert`). `VulkanShader` compiles them with shaderc when it is created, so there is no separate compile step to run. The quad shader's `sampler2D inTextures[32]` and its `switch` must match `Renderer2D`'s texture slot count (`VulkanDescriptorBuilder::MaxImageArraySize`, checked by a `static_assert`).

- **Build-time check.** Oxygen's project also runs `glslc` on every shader as a Visual Studio build step, so syntax errors show up in the Error List. The `.spv` files it writes are not used at runtime yet; they are there for shipping precompiled shaders later.
- **Shader library** (M4a, `Platform/Vulkan/VulkanShaderLibrary.h`): every renderer shader is a vertex + fragment pair loaded by name: `Sprite` (the quad shader), `SpriteCutout`, `Light`, `Shadow`, `Composite`, `Mesh` (Renderer3D). `Load(name, vertex, fragment)` compiles and keeps it (throws when it doesn't compile), `Get(name)` returns it. Pipelines don't keep shaders (their modules are only read at creation), so the code that built a pipeline rebuilds it when its shader changes.
- **Hot reload.** `RendererSettings::ShaderHotReload` (on by default, off in Dist; toggle it in **Render Settings**). Every 250 ms `Renderer2D` calls `VulkanShaderLibrary::ReloadChanged`, which polls each shader's sources and `#include`d files (one `FileWatcher` per shader) and compiles the changed ones again; a shader that doesn't compile keeps its old version and is tried again on its next save. For each shader that took over, only what was built from it is rebuilt: the quad, light, shadow or composite pipeline, and the cached view pipelines of that shader (`ClearViewPipelines(name)`); the rest keeps drawing. The old pipelines go through the deletion queue, so nothing waits for the GPU.
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
| A4 | `Scope` instead of `Ref` for owned GPU objects | **Done (Oct 20, 2026).** No `Ref<Vulkan...>` members are left: owners hold `Scope<T>`, users get `T&` (`Renderer::GetVulkanDevice()`, `GetRendererContext()`, `GetPhysicalDevice()`, `GetVulkanSwapChain()` return references); `PipelineSpecification` points at its shader and descriptor sets without owning them |
| A5 | `SystemContext` and an ordered list of stages in `Scene` | **Done (Oct 24, 2026).** Finishes E5. See [Scene, ECS and systems](SceneAndECS.md#system-context-and-stages) |
| A6 | One component registry | Name, save/load and inspector drawing per component, so saving, the play copy and the inspector loop over one list (E4). See [Scene, ECS and systems](SceneAndECS.md#planned-component-registry) |
| A7 | No Vulkan in `Renderer/` headers | `Renderer.h` and `Renderer2D.h` include `vulkan.h`. `RenderView.h` is clean since M1c (returns an `ImTextureID`; its GPU state is in `VulkanRenderViewData.h`) |
| A8 | No asset paths in the engine | Partly (Oct 20, 2026): the white texture is made in code. `Renderer2D` still loads `assets/shaders/vertex.vert` and `fragment.frag` from the application; engine shaders come from an engine resources folder or a shader library. Needed by the Runtime (RT1) |
| A9 | Smaller fixes | `LayerStack` owns layers as `Scope<Layer>` (no raw `new`/`delete`); `VK_CHECK_RESULT` asserts in Debug ([#35](https://github.com/joelnl74/Hydro/issues/35)); typed event dispatch and `Handled` ([#38](https://github.com/joelnl74/Hydro/issues/38)); `Entity` asserts (E6) |
| A10 | Naming | `m_PascalCase` members (`m_uniformBuffers`, `m_format`, `m_renderPass`, `m_commandBuffer`, `m_vmaAllocation`, `m_memory`, `m_layoutBinding`, `m_poolSizes`); PascalCase spec fields (`ShaderInformation::path`; drop the duplicate `shaderStage`/`shaderType`; `PipelineSpecification::Shader` is done); `HierachyWindow`, `heigth`, `GetPipeLine` (`VullkanTexture` is gone); `enum class` instead of `typedef enum class` (`KeyCodes.h`, `Input.h`) |
| A11 | Const and ownership in signatures | `const Ref<T>&` parameters (`VulkanDescriptorBuilder::BindBuffer` takes `const VulkanUniformBuffer&` since Oct 20, 2026), `explicit` single-argument constructors, `const` overloads of `Entity::GetComponent`/`HasComponent`, `"Hydro/..."` include paths instead of `"../Scene/..."` |

## Asset system

**Steps 1 and 2 of AS1–AS5 are done (Oct 24, 2026): asset identity, and loading textures through the manager.** Every texture, scene, prefab and sprite animation set in the open project's asset folder has an `AssetHandle` that survives renames and moves. Sprites and animators are the first users: scenes save their texture and animation set by handle (below). Textures are loaded and shared by the manager (below); components still hold a `Ref<Texture2D>` / `Ref<SpriteAnimationSet>` (`TODO(assets)`), and animation sets still load through `SpriteAnimationSet::LoadFromFile`.

**Where handles live** (decided Oct 24, 2026, instead of one `AssetRegistry.hyr`). The files are the source of truth, so moves outside the editor and git merges keep working:

- Files Hydro writes keep their handle inside: `.hyscene`, `.hyprefab`, `.hyanim` and `.hymat` start with `Handle: <uint64>`. `SceneSerializer` / `PrefabSerializer` get it from `AssetManager::ImportAsset`, which keeps a file's handle when it is saved again and gives a new file (Save As) a new one; loading ignores the line.
- Other files (images) get a sidecar next to them, `<file>.hymeta` (YAML: `Handle`, `Type`; other keys, such as texture import settings, are kept when the handle is written). Move or rename an image together with its sidecar and the handle stays. Commit `.hymeta` files. The Project window hides them and deletes them with their image.
- The `AssetRegistry` (handle → `AssetMetadata { Handle, Type, Path, IsMissing }`, and path → handle) only lives in memory and is rebuilt by scanning. `AssetMetaSerializer` reads and writes the handles.

**Scanning** (`AssetManager::OnProjectOpened`, `Rescan`) walks the asset folder (hidden files and folders skipped) and reads each file's handle (the first line of a scene or prefab, an image's sidecar):

- a file without one gets a new handle, written into it or its sidecar. The first time a project opens, every scene and prefab gains a `Handle:` line and every image a `.hymeta`;
- two files with the same handle (one was copied): the one at the path registered before keeps it, otherwise the first path in alphabetical order; the other gets a new handle and a warning is logged;
- a registered file that is gone stays in the registry, marked missing, so a reference to it can be reported instead of turning into "none";
- a `.hymeta` without its file is logged (moved without it?).

Oxygen calls `OnProjectOpened` when it opens or creates a project and `OnProjectClosed` in `OnDetach`. The Project window calls `Rescan` when the folder it shows changes (in the editor or in Explorer), and its tooltips show each file's handle.

**`AssetManager`** is a static subsystem started after `Renderer` (it creates GPU textures): `OnProjectOpened(assetDirectory)` / `OnProjectClosed()` (it takes the folder, not the `Project`, so the asset layer doesn't depend on `Project`), `Rescan()` → `AssetScanInfo`, `ImportAsset(path)`, `FindHandle(path)`, `GetMetadata(handle)`, `IsAssetHandleValid(handle)`, `GetAbsolutePath(handle)`, `GetRegistry()`, `MoveAsset(from, to)`, `DeleteAsset(path)` (also by handle), and for loading `GetAsset<T>`, `GetLoadedAsset<T>`, `ReloadAsset<T>`, `GetLoadState(handle)`.

**Loading textures (step 2).** `GetAsset<T>(handle)` loads synchronously and is for scene loading and the editor; `GetLoadedAsset<T>(handle)` returns the cached object or null, never touches the disk and never logs, and is the call render and animation code use every frame. A texture is loaded by `TextureImporter::Import` (reads the file, applies the image's `.hymeta`: colour key, slicing of a sheet that has settings but no sprite list; no Vulkan, no GPU) and uploaded by `Texture2D::Create`. The manager keeps loaded assets in a cache of strong `Ref`s until the project closes. An entry remembers the file's and the `.hymeta`'s modification times and the registry path it was loaded from: `GetAsset` loads again when either changed (so saving new import settings re-imports into the same handle; holders of the old texture keep it), and `ReloadAsset` forces it. A failed load is cached and logged once per file version, so a broken file doesn't spam the log every frame; when an earlier version loaded fine, that object stays in use (state Failed) instead of the sprite going blank. There is no unloading while the project is open, because scenes refer to textures by handle and so a cached object is in use (real unloading needs reference counting, AS7); a rescan drops entries whose file is gone, and the Project window loads thumbnails with `Texture2D::LoadUnshared` so they never fill the cache. `GetLoadState` says NotLoaded, Loaded, Failed or Missing. `Texture2D::LoadFromFile(path)` still exists and asks the manager for files in the asset folder, so every older caller shares the cache; for files elsewhere it loads and shares them while alive. Besides textures the manager loads `SpriteAnimationSet` (`.hyanim`) and, since Oct 25, 2026, `Prefab` (`.hyprefab`, the saved entity kept as YAML for `PrefabSerializer::Instantiate` and `Vfx::Play`) `MaterialAsset` (`.hymat`, M4b, with its textures) and `Mesh` (`.gltf` / `.glb`, M5: imported, with its materials and textures).

**Moving and deleting (step 4).** The editor changes asset files through the manager, never with raw file-system calls. `MoveAsset(from, to)` renames or moves a file or folder inside the asset folder (absolute paths, or relative to it) and keeps every handle: a scene's or prefab's handle is its first line and moves with it, an image's `.hymeta` is moved along with the image, and the registry entry gets the new path, so everything that refers to the asset by handle (sprites, animators, the project's start scene) follows. A folder is renamed and scanned again. It refuses, with a log line and nothing changed, a target that exists, a path outside the asset folder, and an extension that would turn the file into another kind of asset. `DeleteAsset(path)` removes a file with its `.hymeta` (or an empty folder), drops its loaded object and keeps the handle in the registry marked missing, so a scene that uses it shows a missing asset instead of quietly forgetting it (and the handle stays in the scene when it is saved again). The Project window's rename and delete use both.

**The start scene** is `ProjectConfig::StartScene`, an `AssetHandle` saved as `StartSceneHandle` in the `.hyproj`. A project file from before handles has `StartScene: <path>`; `ProjectSerializer` reads it into `ProjectConfig::LegacyStartScene` (and writes it back only while there is no handle), and Oxygen turns it into a handle with `ImportAsset` once the registry exists, then saves the project. `Project` no longer has `GetStartScenePath`: Oxygen asks `AssetManager::GetAbsolutePath(handle)`. The editor's last scene stays a path (an editor setting, and the scene may not be in any project). `EntitySerializer` no longer includes `Project.h`: an old saved `Texture` / `Animations` path goes straight to `ImportAsset`, which resolves relative paths against the asset folder.

**The editor and assets (step 5).** Asset references in the editor are chosen from the registry, not from file dialogs. `AssetPicker::DrawField` (Oxygen) draws the field for one reference: the asset's file name, a popup that lists the project's assets of one `AssetType` (searchable, with small thumbnails for images from `ThumbnailManager`), an X to clear, and a drop target that only accepts assets of that type. It reports the choice; the inspector checks that the asset loads (`GetAsset`, which logs why not) and stores the handle. The sprite's texture, the animator's `.hyanim` and the Animation window's sheet use it, so a file outside the asset folder can't be referenced any more. Drag and drop carries the asset's handle (`AssetDragDrop`): the viewport and hierarchy resolve it to the file's path to open a scene or instantiate a prefab, the pickers use the handle directly. The Project window can rename any registered asset (images and animation sets too, not only scenes and folders) through `MoveAsset`, because every reference is a handle. The windows that remember a path follow a rename (`PathFollower`: open scene, last scene, the asset the inspector shows, the Sprite Editor's image, the Animation window's file) and let go of a deleted file instead of writing it back.

**Thumbnails and the Project window (step 5b).** The Project window lists the registry, not the folder: every file it shows is a registered asset (its tooltip shows the handle), and the folder on disk only supplies the sub-folders and tells it when files were added, removed or renamed, which makes it call `AssetManager::Rescan`. A file that is not an asset (a `.txt`, say) is not listed. Images show a small copy made by `ThumbnailManager` (Oxygen): the image is decoded once by `TextureImporter::Import`, averaged down to at most 128 pixels by `ThumbnailBaker` (colours weighted by alpha, so transparent edges don't darken) and saved as `<project>/.hydro/thumbnails/<handle>.thumb`; next time the editor reads that file instead of decoding the image. The file records the image's size and date and its `.hymeta` date, so a changed image or new import settings make a new one (checked about once a second while a tile is drawn). It is keyed by handle, so a renamed image keeps its thumbnail. At most a few thumbnails are made per frame and the rest show a placeholder, so a folder of hundreds of images fills in instead of freezing. The thumbnail textures belong to the editor (`ThumbnailManager::Clear()` when another project opens), not to the AssetManager's cache. `.hydro/` is a cache that can be deleted at any time and is in `.gitignore`. A prefab's tile shows the first frame of its animation (the same frame the inspector shows: `SpriteAnimationSet::FindStartClip`, first frame of that clip, with the clip's flip), else its sprite's image, else a coloured quad when the sprite has no image; a blue strip still marks it as a prefab. An animation set's tile (purple strip) shows its first frame. These previews need the sheet at full size (sprite regions are small), so they load it through the AssetManager, a couple per frame.

**`Asset`** (`Asset/Asset.h`) is the base of the types the manager will load and hand out as `Ref<T>`: `Texture2D` derives from it (`GetStaticType()`, `GetType()`). It holds only the handle: no paths, no Vulkan. Not every asset type is a loaded object: a scene is a file with a handle, but opening it always builds a fresh `Scene`, which is never shared or cached.

Next steps, in order:

2. **Texture slice. Done (Oct 24, 2026)**, see "Loading textures" above. Still to do from it: file reads go through one small internal source so a pack (RT8) can replace the folder; `TextureImporter` reads the file itself today.
3. **Components store handles. Done (Oct 24, 2026).** `SpriteRendererComponent::Texture` and `SpriteAnimatorComponent::Animations` are `AssetHandle` (0 = none). `SceneRenderer` and `UpdateSpriteAnimations` resolve them with `GetLoadedAsset` (null: a white quad, or the animator is skipped). `EntitySerializer` writes only `TextureHandle` / `AnimationsHandle` (plus a `Sprite` name) and still reads the old `Texture` / `Animations` path keys, importing the file to get a handle; loading a scene preloads what it references with `GetAsset`. The inspector picks files through `ImportAsset`, and the editor's reimport callbacks reload through the manager. Sprites show white until their texture is loaded, textures made in code can't be referenced by a component, and a legacy path-only entry whose file is gone loses its reference when saved again. `EntitySerializer` still includes `Project.h` to turn an old saved path into a file (removed with step 4).
4. **Scenes and prefabs by handle. Done (Oct 24, 2026)**, see "Moving and deleting" and "The start scene" above: `MoveAsset` / `DeleteAsset`, the start scene as a handle, `EntitySerializer` without `Project.h`. Not done on purpose: a prefab instance → source prefab link. Nothing would read it yet (it needs apply / revert in the editor), and a link saved without a user is data to migrate later; it belongs with the prefab workflow.
5. **Editor. Done (Oct 24, 2026)**, see "The editor and assets" above. Small cached thumbnails and listing the Project window from the registry were added right after (Oct 24, 2026, "Thumbnails and the Project window"). Left for later: choosing files from outside the project (they must be put in the asset folder first), and removing thumbnail files of deleted assets (they are small and the folder can be deleted).
6. **Hot reload.** Re-import into the same handle: the manager swaps its cache entry and the old image goes through the deletion queue. Safe because only the manager keeps a `Ref<T>` across frames; everything else resolves the handle each frame.
7. **Later.** Sprites as sub-assets (each sprite in an image's `.hymeta` gets its own handle, with the image as parent), async loading on the `JobScheduler` (C3) with placeholders, an asset pack for `Dist` builds (RT7, RT8).

**Sprite code and handles (done with the sprite merge).** `SpriteAnimationSet` is an `Asset` of its own type (`AssetType::SpriteAnimation`, `.hyanim`). `Texture2D::LoadFromFile` and `SpriteAnimationSet::LoadFromFile` call `AssetManager::BindHandle`, so a loaded object knows its handle when its file is inside the asset folder. Scenes save `TextureHandle` / `AnimationsHandle` only, and a `.hyanim` saves `TextureHandle` next to its `Texture` path. Loading prefers a handle the registry knows (so renames and moves are followed) and falls back to an old `Texture` / `Animations` path key (scenes saved before step 3, files outside the project). A sheet's `TextureImportConfig` lives in the same `<image>.hymeta` as the handle: `TextureImportSerializer::Serialize` keeps `Handle`/`Type`, `AssetMetaSerializer::WriteHandle` keeps the `TextureImport` section, and a sidecar with only a handle means default import settings.

Sprites and frames refer to a sheet's sprites by name in files, so re-slicing doesn't break them; with sub-assets (step 7) that becomes a per-sprite handle.


## Known gaps

These are where the code does not yet match the rules above. Fix them when you touch the area rather than copying the pattern. Every open problem is a [GitHub issue](https://github.com/joelnl74/Hydro/issues).

- `Renderer2D` includes `imgui_impl_vulkan.h` to hand the viewport image to ImGui.
- Each `RenderView` has its own quads, so a scene shown in two windows is submitted (and its vertices uploaded) twice.
- A `RenderView`'s ImGui textures stay in ImGui's descriptor pool until ImGui shuts down; the vendored ImGui has no `ImGui_ImplVulkan_RemoveTexture`.
- `Scene::m_Registry` is public; the editor reads it directly ([#42](https://github.com/joelnl74/Hydro/issues/42)).
- The deletion queue only knows the main frame's fence. ImGui windows dragged outside the main window are submitted separately by ImGui (`RenderPlatformWindowsDefault`), so a view image freed or rewritten after a resize could in theory still be sampled by such a window's previous submission. Not seen in practice; fixed properly when the ImGui upgrade lets those windows render through `Renderer`.
- `Renderer.h` and `Renderer2D.h` include `vulkan.h` (A7).
- Render graph v1 is fixed-stage. Forward lighting has sun shadows only (one map, no cascades: shadows end at `ShadowDistance` from what the camera looks at; point and spot lights cast none), no normal maps, no per-light culling (every pixel loops over the view's up to 64 lights) and no tone mapping; sprites have no normals of their own (derived from the quad). Spot lights don't light the 2D light buffer. Pass timings cover up to `MaxTimedPasses` passes per frame. Click-to-select picks one entity (no box or multi-select, no gizmos yet). In SortOrder sorting with shared depth, flat sprites layered above standing ones keep painter's order only among the translucent quads.
- Meshes: no translucent meshes, no culling or instancing (a model's mesh used by several nodes is copied). Models: one entity per file (no hierarchy import yet), static only, editor-only importer; a `.gltf`'s `.bin` or image files changing doesn't re-import it (only the `.gltf` and its `.hymeta` are watched); FBX/OBJ wait for ufbx. Material assets: no undo, no preview render (the tile is a swatch of the albedo thumbnail and tint), one type (Standard).
- `Renderer2D` loads its shaders from the application's `assets/` folder (A8).
- Sprites reference textures directly (`Ref<Texture2D>`) and scenes save the file path; `EntitySerializer` loads textures and reads `Project` for the asset folder. A texture file that is missing when a scene loads is logged and the sprite draws untextured; saving the scene then drops the reference. Sampling options (`TextureProperties`) aren't saved per texture yet (always linear + repeat). All of this moves to the asset system ([Asset system](#asset-system)).
- A texture re-imported after its `.hymeta` changes is a new `Texture2D` under the same handle: the manager reloads it, components follow because they hold the handle, and anything else holding the old one (a game's own `Ref`) keeps it; the editor only remaps each sprite's index by name.
- Editor previews (`Renderer2D::GetImGuiTexture`) share the deletion-queue caveat of ImGui windows dragged outside the main window (below).
- The 32 texture slots are fixed (shader array size). `Renderer2D::Init` logs an error if the GPU allows fewer samplers per stage (Vulkan guarantees 16; desktop GPUs have far more).
- `AssetManager::Rescan` is synchronous: it reads the first line of every scene and prefab and every sidecar. The Project window runs it only when the folder it shows changes, so a file added in Explorer to another folder gets its handle when that folder is shown, something is saved there, or the project is opened again.
- Textures load synchronously on the main thread (stb_image decode + one upload that waits for the GPU).
- `LayerStack` owns layers through raw pointers (A9); some members and spec fields don't follow the naming rules (A10).
- Shader hot reload rebuilds the pipeline from its existing `PipelineSpecification`, so a shader whose descriptor bindings or vertex inputs change still needs `Renderer2D`'s setup changed and a restart. Only `Renderer2D`'s own shaders (quad, light, shadow, composite) are watched; there is no shader library yet.
- 2D lighting lights the whole view (the clear colour and every sprite): there is no unlit layer or emissive sprite yet (unlit particles use the overlay: on top of everything, no sorting against sprites). Shadows are only blobs under sprites (`Shadow2DComponent`): nothing is cast away from a light, sprites do not shadow each other, and there is no shadow from tiles or walls (D1 in the Roadmap: shadow casters). The editor views are lit too, so a scene with only point lights is dark while editing (add a global light or turn off **Lighting** in **Render Settings**). The light buffer is full resolution; a half-resolution option is not built. Light shader files are loaded from the application's `assets/` folder (A8).
- `Platform/Vulkan/VulkanCommandBuffer.h` and `.cpp` are empty since one-off uploads moved to `VulkanDevice::SubmitImmediate` (Oct 20, 2026); delete them and regenerate the projects.
- `VulkanDevice` and `VulkanSwapChain` still do their setup in the constructor (+ `Create`) instead of `Init` (see [Subsystems](Subsystems.md#known-gaps)).
