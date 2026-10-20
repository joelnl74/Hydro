# Subsystems: startup, shutdown and naming

Every engine-wide service in Hydro (logging, time, events, the window, the renderer, and later audio, scripting and assets) is a **subsystem**. Subsystems are started and stopped explicitly, in one place, in a fixed order. Nothing important happens in a constructor or destructor, so the order is always visible in the code and never depends on when C++ happens to destroy a static or a member.

## Vocabulary

Use these words, in code, comments, docs and pull requests, with these meanings only.

| Term | What it is | Lifetime | Examples |
| --- | --- | --- | --- |
| **Application** | The program. Owns the subsystems and the layer stack, runs the loop | `Init` → `Run` → `Shutdown`, called by `main` | `Application`, `OxygenEditorApplication` |
| **Subsystem** | An engine-wide service. One per application, usually a static facade | `Init` / `Shutdown`, called by `Application` in a fixed order | `Log`, `Time`, `EventDispatcher`, `Window`, `Renderer` |
| **Part** | A piece a subsystem owns and starts itself | `Init` / `Shutdown`, called by its owning subsystem | `VulkanRendererContext`, `VulkanFrameContext`, `VulkanAllocator`, `Renderer2D`, `Renderer3D` |
| **Layer** | An application feature in the layer stack | `OnAttach` / `OnDetach`, called by the layer stack | `EditorLayer`, `ImGuiLayer` |
| **System** | An ECS function over a `registry.view<...>()`, run every frame. No state, no lifetime | none | `UpdateSpin`, the script system in `Scene::OnUpdateRuntime` |
| **Resource** | A GPU or engine object made from a `Specification` | constructor / destructor (RAII), see [Resource creation](ResourceCreation.md) | `VulkanImage`, `VulkanPipeline`, `VulkanTexture` |
| **Hook** | A virtual the engine calls on client code. Named `On...` | called by its owner | `Application::OnInit`, `Layer::OnUpdate` |

Two rules follow from the table:

- **"System" means an ECS system and nothing else.** An engine service is a *subsystem*. Never name a subsystem `...System`.
- **Resources use RAII; subsystems and parts don't.** A resource frees its handles in its destructor, because there are many of them and their owner already decides when they die. A subsystem is started and stopped by name, because the order between subsystems matters and must be readable.

## Subsystem rules

1. **Two lifecycle functions, spelled `Init` and `Shutdown`.** Not `Create`, `Startup`, `Initialize`, `Destroy` or `ShutDown`.
   - `Init` takes its dependencies as parameters (`Renderer::Init(Window&)`), plus a `Specification` if it is configurable (`Window::Init(const WindowSpecification&)`). It never reaches for another subsystem that the caller didn't start first.
   - `Shutdown` releases everything `Init` acquired and leaves the subsystem as if `Init` had never run. Calling it when `Init` didn't run (or failed half-way) is a no-op.
   - `Init` asserts it hasn't run already (`HY_CORE_ASSERT`).
2. **Constructors and destructors do no work.** No allocation of OS or GPU handles, no logging, no calls into other subsystems. A destructor may assert that `Shutdown` ran; it never cleans up on its behalf.
3. **`Application` is the only caller** of a subsystem's `Init` and `Shutdown`, in `Application::Init` and `Application::Shutdown`. Layers and client code never start or stop subsystems.
4. **Shutdown runs in exactly the reverse order of Init.** Keep the two functions next to each other in the file, numbered the same, so a reviewer can check the mirror at a glance.
5. **A subsystem starts its own parts**, in its own `Init`, and stops them in reverse in its own `Shutdown` (`Renderer` does this for the Vulkan context, swapchain, frames, allocator and sub-renderers). Parts follow rules 1, 2 and 4.
6. **Hooks are named `On...`; actions are not.** `Init` does the work, `OnInit` is the client's chance to react. Don't name an action `OnSomething` or a hook `Something`.
7. **State lives behind the facade.** A static subsystem keeps its data in one `s_Data`/`s_Instance` pointer that `Init` creates and `Shutdown` deletes, so "is it running?" is one null check.

## Startup and shutdown order

`main` (in `Core/EntryPoint.h`) runs:

```cpp
Hydro::Application* app = Hydro::CreateApplication(); // constructor: stores the ApplicationSpecification only
app->Init();
app->Run();
app->Shutdown();
delete app;                                           // destructor: no engine work
```

| # | `Application::Init` | Needs | `Application::Shutdown` (reverse) |
| --- | --- | --- | --- |
| 1 | `Log::Init()` | — | `Log::Shutdown()`, last, so every step can still log |
| 2 | `Time::Init()` | — | `Time::Shutdown()` |
| 3 | `EventDispatcher::Init()` | — | `EventDispatcher::Shutdown()` |
| 4 | `Window::Create()`, then `Window::Init(spec.MainWindow)` | Events (window callbacks post events) | `Window::Shutdown()`, then the `Scope` is reset |
| 5 | `Renderer::Init(window)` | Window (surface) | `Renderer::Shutdown()` |
| 6 | Push `ImGuiLayer` as an overlay (`OnAttach`) | Renderer, Window | `Renderer::WaitIdle()`, then `LayerStack::Clear()` detaches and deletes every layer, top-most first |
| 7 | Client `OnInit()` (pushes the application's layers) | everything above | Client `OnShutdown()`, first, while everything is alive |

Inside `Renderer::Init`, and reversed in `Renderer::Shutdown`:

| # | Part | Owns |
| --- | --- | --- |
| 1 | `VulkanRendererContext::Init` | Instance, debug messenger, surface, device |
| 2 | `VulkanSwapChain` (constructor + `Create`) | Swapchain, present render pass |
| 3 | `VulkanFrameContext::Init` | Per-frame command buffers, fences, semaphores |
| 4 | `VulkanAllocator::Init` | VMA allocator |
| 5 | `Renderer2D::Init` | Quad pipeline, shader + hot reload, shared index buffer, white texture, timestamps. Targets, cameras and batches belong to `RenderView`s, which the application creates |
| 6 | `Renderer3D::Init` | Nothing yet: a placeholder for the 3D path (Roadmap R9) |

Shutdown waits for the GPU to be idle before anything GPU-side is released (`Renderer::WaitIdle()` before the layers are detached, and again at the start of `Renderer::Shutdown`). GPU resources released along the way (render views and textures in `OnDetach`, `Renderer2D`'s pipeline and buffers) hand their handles to the renderer's deletion queue; `Renderer::Shutdown` runs it after the layers are gone and again after `Renderer2D` / `Renderer3D` shut down, before the allocator and the device are destroyed. A GPU resource must not outlive `Renderer::Shutdown`.

## The Time subsystem

`Time` (`Core/Time.h`) is the frame clock and the frame's CPU timings. Ask it, instead of keeping your own clock:

```cpp
float dt = Time::GetDeltaTime();                  // seconds, clamped to Time::MaxDeltaTime (0.1 s)
uint64_t frame = Time::GetFrameCount();
double uptime = Time::GetUptime();                // seconds since Time::Init

const FrameTimings& t = Time::GetFrameTimings();  // the last complete frame
float renderMs = t.GetPhaseMs(FramePhase::Render);
float realDeltaMs = t.DeltaMs;                    // unclamped
```

`Application::Run` calls `Time::BeginFrame`/`EndFrame` around each loop iteration and wraps each phase in `Time::BeginPhase`/`EndPhase`. The renderer reports the `Render` (fence wait, acquire, recording) and `Present` (submit, present) phases itself. `Layer::OnUpdate(deltaTime)` receives `Time::GetDeltaTime()`.

GPU-side numbers (draw calls, quads, GPU pass time, memory) stay with the renderer that produces them: `Renderer2D::GetStats()`.

## Naming subsystems

A subsystem is named after the job it does, as a noun, with the suffix chosen by **what kind** of job it is:

| Kind | Suffix | Use when | Names |
| --- | --- | --- | --- |
| Domain | none | It *is* the domain: a facade over one concept | `Log`, `Time`, `Input`, `Window` |
| Doer | agent noun (`-er`, `-or`) | It performs one job on what is submitted to it | `Renderer`, `EventDispatcher` |
| Runtime | `Engine` | It steps a simulation or hosts a foreign runtime every frame | `AudioEngine`, `ScriptEngine`, `PhysicsEngine` |
| Registry | `Manager` | It owns a collection of things that others look up by handle or ID | `AssetManager` |

And:

- **Never `System`** (reserved for ECS systems). Not `Service`, `Handler`, `Helper`, `Controller` or `Module` either: they say nothing about the job.
- **Platform implementations** are `<Platform><Name>`: `GlfwWindow`, `VulkanRendererContext`, `VulkanAllocator`; per-OS files `<OS><Name>` (`MacOSFileDialogs.cpp`). Engine code talks to the platform-free name.
- **Configuration** follows [Resource creation](ResourceCreation.md#naming): `<Name>Specification` passed to `Init` (`WindowSpecification`, `ApplicationSpecification`) and fixed for the subsystem's lifetime; `<Name>Settings` for live options changed while running through `Get<...>Settings` / `Set<...>Settings` (`RendererSettings`); `<Name>Config` only for what is saved to disk (`ProjectConfig`, which holds a `RendererSettings`).
- **Capabilities** a subsystem discovers at `Init` (what the hardware supports) are `<Name>Capabilities` (`RendererCapabilities`), read-only.
- **Per-scene counterparts** of a subsystem take the `Scene` prefix or suffix (`SceneRenderer` submits one scene to the `Renderer`; a future `PhysicsScene` would be one scene's world inside the `PhysicsEngine`). They are owned by a layer or scene, not by `Application`.

### Names we checked

Every current name was checked against the table. Two changed:

| Name | Verdict | Why |
| --- | --- | --- |
| `Renderer` | Keep (not `RenderEngine`) | It is a doer: layers submit draw calls and it renders them. `Engine` is for things that step a simulation or host a foreign runtime (audio mixer, script VM, physics world). `Renderer` is also the name every engine and every reader expects |
| `Renderer2D`, `Renderer3D` | Keep | Parts of `Renderer`, named for the kind of drawing they do |
| `EventDispatcher` | Keep | A doer: dispatches posted events to subscribers |
| `Log`, `Time`, `Window`, `Input` | Keep | Domain facades |
| `LayerController` | **Renamed `LayerStack`** | `Controller` says nothing; it is the stack of layers, and the code and docs already called it that |
| `ImGuiLayer` | Keep | A layer, named `<What>Layer` |
| `Project` | Keep | Data the application opens and closes (`Load`/`New`, `Close`), not a subsystem |
| `SceneRenderer` | Keep | Per-scene counterpart of `Renderer` |
| `ScriptRegistry` | New, not a subsystem | A name → script lookup table. It has no lifecycle or OS/GPU resources (a function-local static, filled before scenes load), so it isn't a `Manager` subsystem; becomes part of a `ScriptEngine` if one is added |
| `Texture2D` | New, a resource (Oct 20, 2026) | A texture sprites are drawn with, made from `ImageSpecification` + `TextureProperties`; `LoadFromFile` shares textures by file until the `AssetManager` takes that over. Named for what it is, like `RenderView` |
| `AssetHandle` | Planned, a value | A `UUID` naming an asset in the project's asset registry; components will hold one instead of a `Ref<Texture2D>` |
| `RenderView` | New, a resource | One picture of a scene (target + camera + quads) that `Renderer2D` draws into; not a subsystem or part, the application creates as many as it shows |
| `GlfwWindow` | **Renamed from `WindowsWindow`** (Oct 20, 2026) | It is GLFW's window on every desktop OS, not a Windows one; `Glfw` rather than `GLFW` so it doesn't read like GLFW's own `GLFWwindow` type |
| `Camera`, `SceneCamera`, `EditorCamera` | New, not subsystems | `Camera` is a projection value (and where clip-space conventions live); `SceneCamera` is the per-scene camera data held by `CameraComponent` (`Scene...` prefix); `EditorCamera` is an editor tool the application owns |

### Current and planned subsystems

| Area | Name | Status | Where it goes in the order |
| --- | --- | --- | --- |
| Logging | `Log` (engine + client loggers, terminal, `logs/*.log`, in-memory history for the editor Console) | Done | 1 |
| Frame clock and timings | `Time` | Done | 2 |
| Events | `EventDispatcher` | Done | 3 |
| Window and OS input | `Window` (`GlfwWindow`, every desktop OS) | Done | 4 |
| Input queries | `Input` | Stateless today, no lifecycle. Becomes a subsystem (after `Window`) when it gains state such as gamepads or action maps | after 4 |
| Rendering | `Renderer` (parts: `Renderer2D`, `Renderer3D`, `Vulkan*`) | Done | 5 |
| Jobs / threads | `JobScheduler` | Planned | after 2: others may submit jobs from their `Init` |
| Assets | `AssetManager` | Planned | after `Renderer` (GPU assets need the device) |
| Audio | `AudioEngine` | Planned | after `AssetManager` |
| Scripting | `ScriptEngine` | Planned | after `AssetManager`, before the client's `OnInit` |
| Physics | `PhysicsEngine` (per scene: `PhysicsScene`) | Planned | after `ScriptEngine` |

## Adding a subsystem

- [ ] Name it with the table above (no `System` suffix)
- [ ] `static void Init(<dependencies>, const <Name>Specification& = {})` and `static void Shutdown()`; constructor and destructor do no work
- [ ] `Init` asserts it hasn't run; `Shutdown` is a no-op if `Init` didn't run
- [ ] Called from `Application::Init` and `Application::Shutdown` at mirrored positions, with a numbered comment
- [ ] The order tables above updated

## Known gaps

- `VulkanDevice` creates the logical device in its constructor, and `VulkanSwapChain` uses its constructor plus `Create` instead of `Init`. Move them to `Init` when you touch them.
- `Core/DeltaTime.h` is unused since `Time` exists. Delete it.
