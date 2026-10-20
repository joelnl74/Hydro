# Hydro Engine Oxygen Editor
Hydro is an early-stage interactive application and rendering engine using Vulkan, for Windows and macOS. Check out the development branch for the latest in development features.

## Getting Started

| | Windows | macOS (Apple Silicon) |
| --- | --- | --- |
| Tools | Visual Studio 2022, Python 3, git | Xcode command line tools (`xcode-select --install`), Python 3, git |
| Vulkan SDK | Installed by the setup script (1.3 or newer) | [LunarG macOS SDK](https://vulkan.lunarg.com/sdk/home#mac) (1.4 recommended), installed by you |
| Setup | `scripts\Setup.bat` | `scripts/Setup.sh` |
| Project files | `scripts\Win-GenProjects.bat` → `Hydro.sln` | `scripts/Mac-GenProjects.sh` → Makefiles (`--xcode` for an Xcode project too) |
| Build and run | Open `Hydro.sln`, build, run Oxygen | `scripts/Build.sh` then `scripts/Run.sh` (both take `Debug`, `Release` or `Dist`) |

Start by cloning the repository with `git clone --recursive https://github.com/joelnl74/Hydro`. If it was cloned non-recursively, run `git submodule update --init`.

### Windows

1. Run `scripts\Setup.bat`. It downloads premake and, if it is missing, the Vulkan SDK installer; install the SDK and run `Setup.bat` again.
2. The setup ends by running `scripts\Win-GenProjects.bat`, which generates `Hydro.sln`. Rerun that script after adding or removing files (building also does it, through the Premake project).
3. Open `Hydro.sln`, build, and run **Oxygen**.

### macOS

1. Install the Xcode command line tools: `xcode-select --install`.
2. Install the Vulkan SDK for macOS from [LunarG](https://vulkan.lunarg.com/sdk/home#mac) (default folder `~/VulkanSDK/<version>`). It contains MoltenVK and KosmicKrisp, which run Vulkan on Metal.
3. Make the SDK available in every terminal: add `source ~/VulkanSDK/<version>/setup-env.sh` to `~/.zshrc` and open a new terminal (`echo $VULKAN_SDK` should print the SDK's `macOS` folder).
4. Run `scripts/Setup.sh`. It downloads premake for macOS, checks the SDK, updates the submodules and generates the Makefiles.
5. `scripts/Build.sh` builds Debug (`scripts/Build.sh Release` for Release), and `scripts/Run.sh` starts Oxygen from the `Oxygen/` folder, where its assets are.
6. After adding or removing source files, run `scripts/Mac-GenProjects.sh` again. `scripts/Mac-GenProjects.sh --xcode` also writes an Xcode project; in Xcode, set the run scheme's working directory to `Oxygen/`.

Linux builds and runs the same way (`scripts/Setup.sh`, `scripts/Build.sh`, `scripts/Run.sh`) and is used for checks, but isn't a supported target yet.

## Documentation
- [Architecture](docs/Architecture.md): projects, platforms, source layout, the frame, Vulkan ownership, conventions
- [Resource creation](docs/ResourceCreation.md): how resources are described and created (Parameter Object pattern)
- [Scene, ECS and systems](docs/SceneAndECS.md): entities, components, systems, cameras, play mode and scripts
- [Subsystems](docs/Subsystems.md): startup and shutdown order, lifecycle rules, naming
- The [wiki](https://github.com/joelnl74/Hydro/wiki) mirrors these and adds the component reference, known issues and the roadmap

## What works today
- Vulkan 2D renderer: batched quads, several render views per frame, shader hot reload, wireframe, GPU timings
- Scenes with an ECS (EnTT): transforms, sprites, cameras, C++ native scripts; saved as YAML scenes and prefabs
- Oxygen editor: Editor and Game views, play / pause / step / stop, hierarchy, inspector, project browser, console, stats and settings windows
- Projects (`.hyproj`) with their own render settings and start scene

## Main features to come:
- Fast 2D rendering (UI, particles, sprites, etc.)
- High-fidelity Physically-Based 3D rendering
- Native rendering API support Vulkan
- Fully featured viewer and editor applications
- Fully scripted interaction and behavior
- Integrated 3rd party 2D and 3D physics engine
- Procedural terrain and world generation
- Artificial Intelligence
- Audio system

Example image 2d renderer and editor view

<img width="1918" height="1026" alt="Screenshot 2026-10-20 000205" src="https://github.com/user-attachments/assets/b58f0bf9-0ad8-4fef-a4db-531a8177d873" />

