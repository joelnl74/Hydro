# Code map

Where things live and which files a typical change touches. Read this first; open the full docs only for the section you need (links below). Keep it short: one line per file, update it when you add, rename or delete a file.

Rules and reasons: [Architecture](Architecture.md), [Subsystems](Subsystems.md), [Resource creation](ResourceCreation.md), [Scene, ECS and systems](SceneAndECS.md), [Test bed](TestBed.md). Unfinished work is tagged in code: search `TODO(assets)` (and `Known gaps` in each doc).

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
| `Asset/AssetManager.*` | `AssetManager` subsystem: project open/close, scan (new handles, copies, missing files), `ImportAsset`, `MoveAsset` / `DeleteAsset`, `BindHandle`, lookups; texture cache: `GetAsset<T>`, `GetLoadedAsset<T>`, `ReloadAsset<T>`, `GetLoadState` |
| **Renderer** (engine-facing, no Vulkan types in new headers) | |
| `Renderer/Renderer.*` | `Renderer` subsystem: owns context, swapchain, frames; `RenderFrame`; deletion queue (`SubmitResourceFree`); `Get/SetSettings`, `GetCapabilities` |
| `Renderer/RendererSettings.h` | `RendererSettings` (live, saved in `.hyproj`; incl. the 2D lighting and shadow options), `LightFalloffStyle`, `RendererCapabilities` |
| `Renderer/Renderer2D.*` | Quad batching, texture slots + flushes, per-view passes, stats, hot reload, wireframe rebuild; `DrawQuad` with tiling or a `TextureRect` (sprite of a sheet); `DrawLine`/`DrawRect`/`DrawCircle` (thin quads in the same batch; thickness in pixels) and `BeginOverlay`/`EndOverlay` (lines drawn after the lighting composite, so night doesn't darken them); `GetImGuiTexture` for editor previews; 2D lighting (`DrawLight`, `DrawGlobalLight`: per-view light buffer pass + composite) and blob shadows (`DrawShadow`); also implements `RenderView`; `DrawQuad(position, size, rotation, ...)` fast path and the soft-dot particle texture |
| `Renderer/RenderView.h` | One offscreen picture of a scene (target, camera, batches) |
| `Renderer/Texture2D.*` | Texture resource (`ImageSpecification` + `TextureProperties`), `Create` (uploads an `ImportedTexture`), `LoadFromFile` (asks the `AssetManager` for project files, shares other files while alive), sprite list (`GetSprite`, `FindSprite`, `GetSpriteUVRect`), `TextureRect` |
| `Renderer/Image.h` | `ImageSpecification`, `TextureProperties`, image/texture enums |
| `Renderer/TextureImport.h` | `TextureImportConfig` (saved per image in `<image>.hymeta`), `SpriteMode`, `SpriteRegion`, `SpriteSlicingConfig` |
| `Renderer/TextureImporter.*` | Decodes an image file with its `.hymeta` (colour key, slicing) into `ImportedTexture` pixels + spec + config; compiles stb_image's implementation (`STB_IMAGE_IMPLEMENTATION`) for the engine; no GPU |
| `Renderer/TextureImportSerializer.*` | `.hymeta` read/write |
| `Renderer/SpriteSlicer.*` | Sprite sheet slicing: `SliceGrid`, `DetectGrid` (pure, no GPU) |
| `Renderer/SceneRenderer.*` | Submits a `Scene`'s sprites, `Light2DComponent`s and `Shadow2DComponent`s through the editor camera or the primary camera (sheet sprites: UV rect, pivot, flip); `RenderEditor` also draws the `EditorOverlay` (selection outline, collider outlines); particles: lit emitters sorted with the sprites, unlit ones in the overlay |
| `Renderer/Camera.*`, `SceneCamera.h`, `EditorCamera.*` | Projections (`*_ZO`, Y flip), per-scene camera data, editor orbit camera + `EditorCameraSettings`, `Focus`/`Frame` to look at a point |
| `Renderer/Renderer3D.*` | Placeholder part for the 3D path (R9): `Init` / `Shutdown` only |
| **Platform/Vulkan** (backend; applications never include these) | |
| `VulkanRendererContext.*`, `VulkanDevice.*`, `VulkanPhysicalDevice.*` | Instance, surface, device (reached with `Renderer::GetVulkanDevice()`), queues, enabled features, `SubmitImmediate` for one-off uploads |
| `VulkanSwapChain.*`, `VulkanFrameContext.*` | Swapchain + present pass; per-frame command buffer, fence, semaphores (`MAX_FRAMES_IN_FLIGHT = 2`) |
| `VulkanAllocator.*` | VMA wrapper, GPU memory stats |
| `VulkanImage.*` | Image + view + sampler (render targets, textures from pixels) |
| `VulkanTexture2DData.h` | Backend half of `Texture2D` (`GetTexture2DData`) |
| `VulkanRenderPass.*` | Offscreen colour target, one image + framebuffer per frame in flight, `Resize` |
| `VulkanPipeline.*` | Pipeline from `PipelineSpecification` (shader and descriptor sets not owned; `BlendMode` None / Alpha / Additive / Multiply2x); `BindDescriptorSet` |
| `VulkanShader.*` | shaderc compile from GLSL, `#include`, source list for hot reload |
| `VulkanDescriptorBuilder.*` | Layout + pool + one set per frame; `BindBuffer`/`BindImage`/`WriteImages` |
| `VulkanVertexBuffer.*`, `VulkanIndexBuffer.*`, `VulkanUniformBuffer.*` | Buffers; vertex layout types live in `VulkanVertexBuffer.h` |
| `VulkanUtils.h` | `VK_CHECK_RESULT` |
| **Scene** | |
| `Scene/Scene.*` | Registry, `CreateEntity`, `OnUpdateRuntime`/`OnUpdateEditor` (build a `SystemContext`, run the stages in order), `Copy` (play mode), primary camera |
| `Scene/SystemContext.h` | `SystemContext`: `DeltaTime`, `FixedDeltaTime`, `FrameIndex`, `IsPlaying`, passed to every engine system |
| `Scene/ScriptSystem.*` | `UpdateScripts` system (creates script instances, runs `OnUpdate`) |
| `Scene/Entity.*` | Handle (`entt::entity` + `Scene*`), component access |
| `Scene/Components/Components.h` | Engine components (incl. `SpriteAnimatorComponent` + its runtime `SpriteAnimatorState`, `Light2DComponent`, `Shadow2DComponent`, `Rigidbody2DComponent`, `BoxCollider2DComponent`, `CircleCollider2DComponent`, `ParticleEmitter2DComponent` + `ParticleBlend` / `ParticleLighting` / `EmitterShape` / `FloatRange`) + `CopyableComponents` |
| `Scene/SpriteAnimation.*` | `SpriteAnimationSet` resource and `Asset` (named clips over one sheet), clip/frame/event types, `LoadFromFile` (shared by file, binds its asset handle) |
| `Scene/SpriteAnimationSerializer.*` | `.hyanim` read/write (handle first line, texture by `TextureHandle` + path) |
| `Scene/SpriteAnimator.*` | `UpdateSpriteAnimations(Scene&, const SystemContext&)` system, `ResetSpriteAnimations` (called by `Scene::OnRuntimeStart`); `SpriteAnimator` API (`Play`, `Stop`, `Pause`, `Resume`, `IsPlaying`, `GetFrameIndex`..., and `ResetPlayback` / `RestartAfterReload` for the editor). The only code that writes `SpriteAnimatorState` |
| `Scene/ParticleScene.*` | `ParticleScene` (the scene's live particles: recycled per-emitter buffers, emission, simulation, `DestroyWhenFinished`, stats), the `UpdateParticles` system (stage 5) and the `Particles2D` API (`Play`, `Stop`, `Burst`, `IsPlaying`, `GetAliveCount`) |
| `Scene/SceneTransition.h` | `SceneTransition`: Push / Pop requests for a scene stack (header only); the host (`EditorLayer::ProcessSceneTransition`) honours them after the frame update |
| `Scene/ScriptableEntity.h`, `ScriptRegistry.*` | `NativeScriptComponent`, script base class, name → factory table |
| `Scene/EntitySerializer.*` | YAML for every component (both directions); used by scenes and prefabs |
| `Scene/SceneSerializer.*`, `PrefabSerializer.*` | `.hyscene` / `.hyprefab` files |
| **Physics** | |
| `Physics/PhysicsEngine.*` | `PhysicsEngine` subsystem: Box2D assert hook and length unit, version, a Debug-build self test (floor + falling box) |
| `Physics/PhysicsScene.*` | `PhysicsScene`: the Box2D world of one running `Scene` (created by `Scene::OnRuntimeStart`, destroyed on stop). Bodies from Rigidbody2D / collider components, 60 Hz fixed steps (max 4 a frame), dynamic results written back to transforms, kinematic bodies carried to their transforms; `Physics2D::SetLinearVelocity` for game code; bodies for entities spawned during play (`CreateNewBodies`, `EnsureBody`). Collision and trigger events are collected after each step and delivered by `Scene::DispatchPhysicsEvents` to `ScriptableEntity::OnCollisionEnter/Exit`, `OnTriggerEnter/Exit`. No interpolation yet |
| **Project, Utils** | |
| `Project/Project.*`, `ProjectSerializer.*` | Active project, `ProjectConfig` (`.hyproj`) |
| `Utils/YAMLConversions.*`, `Utils/PlatformUtils.h` | glm ↔ YAML; `FileDialogs` |
| `Hydro.h` | What applications include |

## Editor: `Oxygen/src`

| File | Owns / does |
| --- | --- |
| `OxygenEditorApplication.cpp` | `CreateApplication`, registers example scripts, pushes `EditorLayer` |
| `EditorLayer.*` | Editor scene + play copy, the two `RenderView`s, `EditorCamera`, all windows, menus and top bar, open/save project and scene, the scene stack for play mode (`ProcessSceneTransition`), asset drops, inspected asset, re-import of textures / reload of animation sets into the open scenes, galaxy demo |
| `EditorSettings.*` | `EditorSettings` + `EditorSettingsSerializer` (`EditorSettings.yaml`, session) |
| `EditorPaths.h` | Project-aware paths (scenes, prefabs, settings) |
| `EditorWindows/InspectorWindow.*` | Component UI, **Add Component**, script list, prefab button; the selected asset when no entity is selected (image import settings, `.hyanim` summary) |
| `EditorWindows/TextureImportPanel.*` | Inspector panel for an image: preview, `TextureImportConfig` (sprite mode, pixels per unit, filter, wrap, colour space, colour key), Apply / Revert |
| `EditorWindows/SpriteEditorWindow.*` | **Sprite Editor**: slice a sheet (grid, detect grid, manual), row names, per-sprite name / rect / pivot, sprites drawn over the image |
| `EditorWindows/SpriteAnimationWindow.*` | **Animation**: edit a `.hyanim` (sheet, clips, frames, durations, loop, flip, events, default clip), live preview, sprite picker; `MakeClipsFromSpriteNames` |
| `EditorWindows/PropertyUI.h` | Shared label/value rows, number fields and `SpriteImage` for the asset editors |
| `EditorWindows/HierachyWindow.*` | Entity list with search box, create/delete, prefab drops, double-click to focus (`OnEntityFocus`) |
| `EditorWindows/ProjectWindow.*` | Asset browser listing the registry (plus folders), drag sources (prefabs, scenes, images, `.hyanim`), prefab previews (first animation frame or sprite image), `.hyanim` first frames, image thumbnails; selects / opens assets; **Create Sprite Animation** from a sheet; hides `.hymeta`; calls `AssetManager::Rescan` when the folder changes, handle in tooltips |
| `EditorWindows/ConsoleWindow.*` | Log view |
| `EditorWindows/StatsWindow.*`, `RendererStatsWindow.*`, `StatsUI.h` | Frame timings; `Renderer2D` stats; shared table helpers; Scene tab: entity counts and the running scene's particle stats |
| `EditorWindows/RenderSettingsWindow.*`, `ProjectSettingsWindow.*`, `EditorSettingsWindow.*` | Live `RendererSettings`; project name/start scene; editor preferences pages |
| `EditorWindows/EditorStyle.*`, `Utilities/EditorChrome.*` | Themes from one accent colour; droplet, waterline, icon buttons, play tint |
| `Utilities/AssetDragDrop.h` | Drag & drop payload (the asset's handle; `AcceptHandle` by type, `AcceptPayload` as a path) |
| `Utilities/AssetPicker.*` | The asset reference field: file name, searchable popup of one asset type with thumbnails, drop target, clear |
| `Utilities/ThumbnailManager.*` | Small image thumbnails by handle for the Project window and picker; a few per frame, cached on disk in `<project>/.hydro/thumbnails` |
| `Utilities/ThumbnailBaker.*` | Averages pixels down to a thumbnail and reads/writes the `.thumb` file; no GPU, no engine types |
| `Utilities/PathFollower.h` | Keeps a remembered path valid after a rename or move |
| `Scripts/ExampleScripts.h` | `SpinScript`, `BobScript`, `VillagerWalkScript` (WASD + `SpriteAnimator::Play`), `YSortScript` (sorts a moving sprite by its Y), `CameraFollowScript` (put it on the camera: glides after the entity tagged "Player"); `VillagerWalk` stands still while `UIState::MenuOpen`; includes `VillageScripts.h`, `BattleScripts.h`, `MvpScripts.h` and registers them all |
| `Scripts/VillageScripts.h` | The `StatueVillage` scene's scripts: `VillageYSortScript` (SortOrder from the feet, 4 x tile y), `WaypointWalkScript` (an NPC walks a loop through `Waypoint <Name> <n> [Facing] [Seconds]` entities, 4 directions, no physics), `VillageCameraScript` (follows the Player, stays inside the 60 x 36 map, zoom with - and =), `VillageDirectorScript` (keys 1/2/3 day/dusk/night, L lights, H shadows, F flicker, B statue/brazier, [ ] radius) |
| `Scripts/BattleScripts.h` | `BattleState` (HP and result shared by the village and the battle), `EnemyChaseScript` (a slime that wanders, chases the player and pushes the `Battle` scene on contact), `BattleScript` (the turn-based battle of `Battle.hyscene`: keys 1 to 4, HP bars as quads); battle particle effects (hit sparks, defeat puffs, heal motes: `HitSparks`, `DefeatPuff`, `HealMotes`, `PlayEffect`) |
| `Scripts/MvpScripts.h` | The `MVP` scene's mechanics: `MvpDirectorScript` (interact prompts and actions on `Sign` / `Chest` / `Door` / `Save` / `Exit` / `Npc` entities, a message banner, the sample inventory menu on Tab that sets `UIState::MenuOpen`, the `UI *` sprites kept on the camera), `MvpLabel` (rows of `DioramaRPG/UI/MvpText.png`) |
| `assets/shaders/vertex.vert`, `fragment.frag` | The quad shader (`inTextures[32]` must match `Renderer2D`) |
| `assets/shaders/light.frag`, `shadow.frag`, `composite.vert`, `composite.frag` | 2D lighting: the light's falloff curve and the blob shadow's softness (both use `vertex.vert`), and the fullscreen triangle that multiplies the light buffer over the scene. Hot reloaded |
| `project/` | The Sandbox project Oxygen opens at startup; `Assets/Characters` (villager sheet, `.hymeta`, `.hyanim`) + `Scenes/StatueVillage.hyscene` is the **gameplay test bed** (start scene; the village and the East Meadow, with lighting, scripts and colliders; the walkable map is `VillageMap` in `Oxygen/src/Scripts/VillageScripts.h`; see `docs/TestBed.md`); `Scenes/Villagers.hyscene` is a smaller sprite-animation example |

Dead files (delete when convenient): `Core/DeltaTime.h`, `Core/LayerController.*`, `Oxygen/src/EditorWindows/ContentWindow.h`. Also `Platform/Vulkan/VullkanTexture.*`, `VulkanBuffer.*` and `VulkanRenderCommandBuffer.*`: emptied on Oct 20, 2026, the files themselves still need deleting.

## Recipes: which files a change touches

| Change | Files |
| --- | --- |
| New engine component | `Components.h` (+ `CopyableComponents`), `EntitySerializer.cpp` (both directions, defaults), `InspectorWindow.cpp` (`DrawComponents` + **Add Component**), wiki `Components.md`, `docs/SceneAndECS.md` table. Runtime state: decide with the component state rule (`docs/SceneAndECS.md`, *Where runtime state lives*): a `State` member reset in `Scene::OnRuntimeStart`, or a per-scene `...Scene` object |
| New field on a component | Same as above minus `CopyableComponents` |
| Test a new system or feature | Add it to the village in `Assets/Scenes/StatueVillage.hyscene` as a gameplay element (a collider on a building, a pushable prop, a zone, a sound source) in the game's own art style, update `VillageMap` if the map grows, and add a row to the Features table in `docs/TestBed.md`; code that exercises it goes in `Oxygen/src/Scripts` |
| New engine system | `(Scene&, const SystemContext&)` function over a view, called as a stage from `Scene::OnUpdateRuntime` (and `OnUpdateEditor` if it must run while editing) in `Scene.cpp`; frame order in `docs/SceneAndECS.md` |
| New renderer setting | `RendererSettings.h`, read in `Renderer2D.cpp`/`Renderer.cpp` (or applied in `Renderer::SetSettings`), `ProjectSerializer.cpp`, `RenderSettingsWindow.cpp`, docs inventory |
| New editor preference | `EditorSettings.h`, `EditorSettings.cpp` (serializer, default on missing key), `EditorSettingsWindow.cpp` |
| New editor window | `EditorWindows/<Name>Window.*`, member + View menu entry in `EditorLayer.*`; callbacks for anything that saves |
| New resource / spec | Spec next to the API (`Renderer/`), backend in `Platform/Vulkan/` (destructor frees through `Renderer::SubmitResourceFree`, owner holds a `Scope`), inventory in `docs/ResourceCreation.md` |
| New subsystem | Class + `Init`/`Shutdown`, `Application.cpp` (mirrored positions), tables in `docs/Subsystems.md` |
| 2D drawing / batching | `Renderer2D.*`, shaders in `Oxygen/assets/shaders`, `RendererStatsWindow.cpp` for new stats |
| Textures | `Texture2D.*`, `VulkanImage.*`, `VulkanTexture2DData.h`; sprites: `SceneRenderer.cpp`, `EntitySerializer.cpp`, `InspectorWindow.cpp` |
| New asset type | `AssetType` + `AssetTypeToString` / `AssetTypeFromString` / `GetAssetTypeFromExtension` (`Asset.cpp`); `AssetMetaSerializer::StoresHandleInFile` if Hydro writes the file (and its serializer writes `Handle` first); a loaded type derives from `Asset` |
| Sprite sheets / import settings | `TextureImport.h`, `TextureImportSerializer.*`, `TextureImporter.*`, `SpriteSlicer.*`, `Texture2D.cpp` (`Create`, `SetImportConfig`) |
| Sprite animation | `SpriteAnimation.*`, `SpriteAnimationSerializer.*`, `SpriteAnimator.*` (system + API), `Scene.cpp` (`OnUpdateRuntime`, `OnRuntimeStart`), inspector section, `EditorLayer.cpp` (reload after the Animation window saves), `SpriteAnimationWindow.*` |
| Particles | `Components.h` (`ParticleEmitter2DComponent`), `ParticleScene.*` (simulation, `Particles2D`), `Scene.*` (owns the `ParticleScene`, stage 5), `SceneRenderer.*` (submission), `Renderer2D.*` (fast `DrawQuad`, soft dot), `EntitySerializer.cpp`, inspector section, `StatsWindow.cpp` (Scene tab) |
| An image shown in the editor | `Renderer2D::GetImGuiTexture(texture)` (pooled ImGui handles), `PropertyUI::SpriteImage` for one sprite |
| Docs | Edit `docs/`, then `python scripts/regen_wiki.py`; wiki-only pages (Components, Roadmap, Known-Issues) by hand; `docs/TestBed.md` is the Test bed page (the wiki's Test-Bed) |

## Gotchas

- **Box2D** is a git submodule at `Hydro/vendor/box2d`, pinned to v3.1.1 (`git submodule update --init Hydro/vendor/box2d`). `Hydro/vendor/box2d.premake5.lua` builds it as a C17 static library; Debug is optimised on purpose (see the comment there). Only `Physics/` includes `<box2d/box2d.h>`.
- **Old EnTT** (vendored single header `Hydro/vendor/entt/entt.hpp`, no version macro): `registry.has<T>`, `view.get<A, B>(e)`; no `all_of`/`any_of` (upgrade is Roadmap E2).
- **Line endings**: CRLF in the working copy for sources and docs (`.gitattributes`); `*.sh` is LF.
- **Style**: `namespace Hydro` only, no `!` (use `== false` / `== nullptr`), braces on every body, `m_PascalCase`. See the skill or [Architecture](Architecture.md#code-conventions).
- `Renderer2D.h`, `Renderer.h`, `RenderView.h` still include `vulkan.h` (A7); don't add Vulkan to other `Renderer/` headers.
- Shaders are compiled at runtime by shaderc from `assets/shaders/...`, relative to the working directory (run Oxygen from the `Oxygen/` folder).

## Checking without Windows

`scripts/check.sh [files...]`: g++ `-fsyntax-only -Wall -Wextra` over the engine and editor sources (Windows-only files skipped) and glslang over the shaders. Linux/WSL; downloads Vulkan-Headers, shaderc headers and builds glslang once into `/tmp/hydro-check`. Not a substitute for an MSVC `/W4` build.

## Tools

`scripts/diorama_rpg/`: the Diorama RPG art and scene edits, on top of `statue_village/lib.py`: `build.py` (runs everything; `--battle` rewrites `Battle.hyscene`, `--no-environment`, `--no-regrade`), `heroes.py` (field sheets), `battle_sprites.py` (96 x 128 battle sheets from a pose rig, effect sheets, per-frame timing), `art.py` (Wisp, battle backdrop, UI sheets), `env.py` (ground and decor), `style.py` (palette), `regrade.py`, `check_assets.py` (art validator), `mvp.py` (the MVP scene: the GDD's first-scene test, built from the existing art, with its interactables and UI entities), `preview_scene.py` (offline mockup).

`scripts/statue_village/`: Python (numpy, scipy, Pillow) that draws the `StatueVillage` pixel art and writes the scene: `gen_village.py` (layout, entities, `.hymeta`/`.hyanim`/`.hyscene` output; handles are hashed from the file path), `lib.py` (canvas with binary alpha), `nature.py`, `build.py`, `props.py`, `chars.py` (villager walk sheets), `ground.py` (grass, paths, plaza, garden, animated pond), `preview.py` (offline lighting mockup).

`scripts/diorama_rpg/`: Python (numpy, scipy, Pillow, PyYAML) for the Diorama RPG test content ([DioramaRPG.md](DioramaRPG.md), [StyleGuide.md](StyleGuide.md)): `style.py` (palette, light, pixel density), `env.py` (ground repaint, house detail pass, set dressing), `regrade.py` (recolours the older village art), `heroes.py` (Orin and Lila), `art.py` (Spirit Wisp, battle backdrop, battle UI sheets and pixel font), `build.py` (writes the assets and edits `StatueVillage.hyscene`; `--battle` also `Battle.hyscene`), `preview_scene.py` (offline mockup of any `.hyscene`).
