# Hydro Engine Oxygen Editor
Hydro is primarily an early-stage interactive application and rendering engine for Windows using Vulkan. Check out the development branch for the latest in development features.

## Getting Started
Visual Studio 2022 or 2019 is recommended, Hydro is officially untested on other development environments whilst we focus on a Windows build.

1. Downloading the repository:

Start by cloning the repository with git clone --recursive https://github.com/joelnl74/Hydro.

If the repository was cloned non-recursively previously, use git submodule update --init to clone the necessary submodules.

2. Configuring the dependencies:

- Run the Setup.bat file found in scripts folder. This will download the required prerequisites for the project if they are not present yet.
- One prerequisite is the Vulkan SDK. If it is not installed, the script will execute the VulkanSDK.exe file, and will prompt the user to install the SDK.
- After installation, run the Setup.bat file again. If the Vulkan SDK is installed properly, it will then download the Vulkan SDK Debug libraries. (This may take a longer amount of time)
- After downloading and unzipping the files, the Win-GenProjects.bat script file will get executed automatically, which will then generate a Visual Studio solution file for user's usage.
- If changes are made, or if you want to regenerate project files, rerun the Win-GenProjects.bat script file found in scripts folder.

## Documentation
- [Architecture](docs/Architecture.md): projects, source layout, the frame, Vulkan ownership, conventions
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

Example image of the 3d and 2d renderer and editor

<img width="1917" height="1030" alt="Screenshot 2026-10-29 062732" src="https://github.com/user-attachments/assets/8d809ca6-54fc-4b96-92d2-6ca16bce0b05" />


