# Hydro Engine

**Hydro** is an experimental C++ game and rendering engine for Windows, built around **Vulkan** and accompanied by the **Oxygen Editor**.

The project started as a Vulkan 2D renderer and has since grown into a broader game-engine project covering scene management, an ECS, asset management, native C++ scripting, editor tooling, 2D gameplay systems, and an expanding 3D rendering stack.

Hydro is primarily a personal engine-development and graphics-programming project. It is under active development and should currently be considered **experimental rather than production-ready**.

> Development currently focuses on Windows, Vulkan, editor tooling, and building the rendering and gameplay technology required for 2D and HD-2D / 3D games.

---

## Current Status

Hydro currently provides a usable engine/editor foundation rather than a finished general-purpose game engine.

The major systems can be roughly grouped as:

- **2D engine:** functional and used for gameplay prototypes.
- **Scene/ECS architecture:** functional.
- **Oxygen Editor:** functional and actively expanding.
- **Asset pipeline:** functional foundation, continuing to evolve.
- **Native scripting:** functional.
- **2D rendering:** functional.
- **3D rendering:** under active development.
- **3D asset/import pipeline:** under active development.
- **Physics:** Box2D-based 2D physics available; Jolt-based 3D physics is being developed.
- **Audio:** planned / early development.
- **Advanced rendering:** lighting, shadows, post-processing and related systems are being expanded incrementally.

---

# Features

## Rendering

Hydro uses a native **Vulkan rendering backend** with explicit control over rendering resources and synchronization.

### 2D Rendering

The existing 2D renderer supports:

- Batched sprite / quad rendering
- Multiple render views per frame
- Cameras and camera components
- Sprite animation
- Textures and materials
- Alpha-tested and transparent sprites
- 2D global lighting
- 2D point lighting
- Basic 2D shadows
- Wireframe rendering
- Shader hot reloading
- GPU timing / profiling information
- Editor and runtime rendering
- Independent editor and game views

The 2D renderer is the most mature rendering path and is used for current gameplay and engine prototypes.

### 3D Rendering

Hydro is being expanded with a native 3D rendering pipeline while preserving the existing 2D workflow.

Current development includes:

- Vulkan dynamic rendering
- Depth-buffered 3D rendering
- GPU vertex and index buffers
- Mesh rendering
- Built-in primitive geometry
- 3D transforms
- Perspective and orthographic cameras
- Material support
- Texture-backed materials
- Directional lights
- Point lights
- Spot lights
- Emissive materials
- Ambient lighting
- Fog
- Editor entity-ID rendering / picking
- glTF / GLB asset importing
- 2D sprites rendered inside 3D scenes
- Cut-out sprite rendering with depth
- Mixed 2D / 3D scenes

A major goal of this work is supporting **3D and 2D combined games / diorama-style rendering**, where sprite characters can naturally interact with 3D environments and lighting.

### Rendering Work in Progress

Areas currently being developed or investigated include:

- Physically based materials
- Improved shadow rendering
- HDR rendering
- Bloom
- Tonemapping
- Improved transparency
- Advanced material properties
- Better lighting scalability
- Render debugging tools
- GPU performance instrumentation
- Improved post-processing
- Environment and atmospheric effects

---

# Oxygen Editor

**Oxygen** is Hydro's editor application.

It runs directly on top of the Hydro engine and is intended to provide a Unity/Godot-style workflow while remaining relatively lightweight and closely integrated with the underlying engine architecture.

Current editor functionality includes:

- Editor viewport
- Game viewport
- Scene hierarchy
- Entity selection
- Component inspector
- Transform editing
- Project browser
- Asset browser
- Console
- Engine statistics
- Render statistics
- Project settings
- Render settings
- Scene creation and loading
- Scene saving
- Prefabs
- Play mode
- Pause
- Single-frame stepping
- Stop / return to edit mode
- Native script inspection
- Runtime/editor separation
- Multiple render views
- Entity picking
- Debug rendering

Editor tooling continues to be one of the major development areas for Hydro.

---

# Scene and ECS

Hydro uses **EnTT** as its Entity Component System.

Scenes are composed from entities and components rather than a rigid inheritance-based object hierarchy.

Existing engine components include systems for:

- Entity identification
- Tags / names
- Transforms
- Cameras
- Sprites
- Sprite animation
- Native scripts
- Rendering
- Lights
- Meshes
- Materials
- Physics-related data

Scenes and prefabs are serialized using **YAML**.

Example:

```text
Scene
├── Camera
├── Environment
├── Player
│   ├── Transform
│   ├── Sprite
│   └── NativeScript
├── Directional Light
└── Props
```

The engine supports a distinction between **editor state** and **runtime/play state**, allowing scenes to be entered, simulated and restored from the Oxygen Editor.

---

# Native C++ Scripting

Gameplay behaviour can be implemented using native C++ scripts attached to entities.

Native scripts integrate with the engine's entity/component architecture and scene lifecycle.

The scripting system supports concepts such as:

- Script components
- Entity-bound behaviour
- Initialization
- Per-frame updates
- Runtime scene execution
- Multiple scripts on an entity
- Script serialization references

The long-term goal is to keep native C++ as a first-class workflow while continuing to improve iteration speed and editor integration.

---

# Asset System

Hydro contains an engine-level asset system rather than loading resources directly throughout gameplay and renderer code.

The asset architecture includes concepts such as:

- Asset handles
- Asset metadata
- Asset registry
- Asset manager
- Project-relative assets
- Runtime asset lookup
- Serialized asset references
- Texture assets
- Scene assets
- Prefab assets
- Material assets
- Mesh / model assets

This provides a common foundation for editor tooling, serialization, importing and runtime resource management.

The asset pipeline is still actively evolving as 3D and more complex content workflows are added.

---

# Projects

Hydro games/applications are represented by `.hyproj` project files.

Projects can define engine/application configuration including:

- Project information
- Asset directory
- Start scene
- Rendering configuration
- Runtime settings

This allows Oxygen to load individual Hydro projects rather than treating the engine repository itself as the game.

---

# Physics

Hydro's physics architecture is being developed around separate 2D and 3D backends.

## 2D

**Box2D** is used for 2D physics functionality.

The goal is to keep the 2D physics workflow lightweight and appropriate for sprite-based projects.

## 3D

**Jolt Physics** is the planned / developing 3D physics backend.

The initial 3D physics work focuses on:

- Physics worlds
- Fixed simulation stepping
- Rigid bodies
- Static bodies
- Dynamic bodies
- Box colliders
- Basic collision layers
- Transform synchronization
- Physics debug visualization

Later work is expected to expand this into additional collider types, queries, character movement and more complete editor integration.

---

# HD-2D / Hybrid Rendering

One of Hydro's current development targets is supporting games that combine traditional sprite-based presentation with 3D environments.

The intended workflow allows:

- Pixel-art characters
- Upright billboard / world-space sprites
- 3D buildings and props
- Real depth testing
- Characters walking behind and in front of geometry
- Directional, point and spot lighting
- Emissive environment elements
- Orthographic or perspective diorama cameras
- Imported glTF environments
- Traditional top-down 2D scenes

This lets projects gradually mix 2D and 3D rather than requiring separate engines or rendering architectures.

---

# Architecture

The repository is separated into the core engine and editor applications.

```text
Hydro/
├── Hydro/              # Core engine
├── Oxygen/             # Oxygen editor
├── docs/               # Engine documentation
├── scripts/            # Setup and project-generation scripts
├── vendor/             # Third-party dependencies
├── premake5.lua
├── Dependencies.lua
└── README.md
```

At a high level:

```text
Application / Game
        │
        ▼
   Scene / ECS
        │
        ├───────────────┐
        ▼               ▼
 Gameplay Systems   Asset System
        │               │
        └───────┬───────┘
                ▼
             Hydro
        ┌───────┼────────┐
        ▼       ▼        ▼
    Renderer  Physics  Platform
        │
        ▼
      Vulkan
```

Oxygen uses these same engine systems rather than maintaining an independent editor-specific representation of scenes.

---

# Getting Started

## Requirements

Hydro currently targets **Windows**.

Recommended development environment:

- Windows 10 / 11
- Visual Studio 2022
- Vulkan-capable GPU
- Vulkan SDK
- Git

Visual Studio 2022 is the primary development environment.

Other compilers and operating systems are currently not officially tested.

---

## 1. Clone the Repository

Clone Hydro recursively so that required Git submodules are also downloaded:

```bash
git clone --recursive https://github.com/joelnl74/Hydro.git
```

If you already cloned the repository without `--recursive`, initialize the submodules with:

```bash
git submodule update --init --recursive
```

---

## 2. Run Setup

Run:

```text
scripts/Setup.bat
```

The setup script installs or downloads required project prerequisites.

The **Vulkan SDK** is required.

If the SDK is not installed, setup may launch the Vulkan SDK installer. After completing the installation, run `Setup.bat` again.

The script will then retrieve any additional required Vulkan development/debug files and generate the Visual Studio project.

---

## 3. Generate Visual Studio Projects

Project files can be regenerated manually using:

```text
scripts/Win-GenProjects.bat
```

Run this again whenever Premake configuration or project structure changes.

---

## 4. Open the Solution

Open the generated Visual Studio solution and build the desired configuration.

Typical development primarily happens through the **Oxygen Editor** executable.

---

# Documentation

More detailed engine documentation is available under [`docs/`](docs/).

Current documentation includes:

- [Architecture](docs/Architecture.md)  
  Project structure, engine ownership, frame execution and conventions.

- [Resource Creation](docs/ResourceCreation.md)  
  Resource creation APIs and the Parameter Object pattern.

- [Scene, ECS and Systems](docs/SceneAndECS.md)  
  Entities, components, systems, cameras, scenes, play mode and native scripts.

- [Subsystems](docs/Subsystems.md)  
  Engine subsystem lifetime, startup/shutdown ordering and naming conventions.

The [Hydro Wiki](https://github.com/joelnl74/Hydro/wiki) contains additional information including component documentation, known issues and roadmap information.

---

# Development Roadmap

Hydro does not currently target feature parity with commercial engines. Development is driven primarily by the requirements of projects being built with it.

## Near Term

### 3D Rendering

- Complete basic mesh rendering pipeline
- Improve glTF / GLB importing
- Expand material system
- Directional / point / spot lighting
- Shadow mapping
- HDR rendering
- Bloom
- Tonemapping
- Improved transparency
- Better editor visualization
- Renderer debug tooling

### Physics

- Integrate Jolt Physics backend
- Rigid bodies
- Collider components
- Physics debug visualization
- Scene serialization
- Editor inspector integration
- Raycasts and scene queries
- Character collision / movement

### Editor

- Better 3D scene editing
- Improved gizmos
- Material inspector
- Mesh/model import workflow
- Physics component editing
- Improved asset import UX
- Better scene diagnostics
- Undo / redo improvements
- Editor quality-of-life tooling

### Asset Pipeline

- More complete model importing
- Asset dependency tracking
- Import settings
- Asset reimporting
- Improved material assets
- Better texture import configuration

---

## Medium Term

- More complete PBR rendering
- Image-based lighting
- Environment maps
- Improved GPU resource management
- Render graph / pass organization improvements
- Particle system
- UI rendering improvements
- Animation system improvements
- Skeletal animation
- Audio system
- Expanded physics support
- Runtime debug tooling
- Improved profiling
- Build / packaging pipeline

---

## Longer Term / Research

Potential future areas include:

- Procedural terrain
- Procedural world generation
- Navigation
- AI gameplay systems
- Compute-driven rendering
- GPU-driven rendering
- Large-world rendering
- Advanced post-processing
- Volumetric effects
- More sophisticated global illumination
- Platform abstraction beyond Windows
- Additional rendering APIs where useful

These should be considered research directions rather than committed features.

---

# Project Goals

Hydro is primarily designed around several principles:

### Learn the systems beneath modern engines

Hydro intentionally exposes graphics, resource management, rendering and engine architecture rather than hiding those systems behind a large abstraction layer.

### Keep Vulkan first-class

Vulkan is not treated as an implementation detail behind a generic graphics abstraction. Hydro can make direct use of modern Vulkan features where appropriate.

### Support both 2D and 3D

2D is not intended to become a compatibility layer sitting on top of a 3D renderer.

Hydro aims to support:

- Traditional 2D games
- Pixel-art games
- 3D games
- Hybrid 2D / 3D projects
- HD-2D / diorama-style games

### Build the editor alongside the engine

Oxygen is developed together with Hydro so engine functionality can be inspected, debugged and authored through real tooling rather than only through test applications.

### Build features when real projects need them

The engine roadmap is intentionally project-driven. Systems are generally expanded when there is a concrete game, renderer or editor use case for them.

---

# Screenshots

## Oxygen Editor / 2D Renderer

<img width="1918" height="1026" alt="Hydro Oxygen Editor" src="https://github.com/user-attachments/assets/b58f0bf9-0ad8-4fef-a4db-531a8177d873" />

## Oxgen Editor / 3D/2D Renderer

More screenshots and examples will be added as the 3D renderer and editor tooling mature.

---

# Development

Hydro is an experimental personal project and APIs may change frequently.

Expect:

- Breaking engine API changes
- Serialization changes
- Renderer architecture changes
- Editor workflow changes
- Incomplete systems
- Limited platform testing

The repository should currently be treated as an engine-development project rather than a stable SDK.

---

# License

Hydro is released under the repository's [MIT License](LICENSE).

Third-party libraries and dependencies remain subject to their respective licenses.
