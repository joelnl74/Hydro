# Hydro: notes for Claude

Vulkan game engine (`Hydro`, static library) and its editor (`Oxygen`). C++17, premake. Windows (MSVC, Visual Studio) and macOS (Apple Clang, Makefiles); Linux builds for checks. Per-OS code lives in `Hydro/src/Hydro/Platform/<OS>` (see `docs/Architecture.md#platforms`).

## Find things cheaply

1. **`docs/CodeMap.md` first**: every file in one line, and which files each kind of change touches. Then open only the section of the full doc you need (`docs/Architecture.md`, `Subsystems.md`, `ResourceCreation.md`, `SceneAndECS.md`); don't read them end to end.
2. Search, don't guess: `grep -rn "Name" Hydro/src Oxygen/src`. Unfinished work is tagged `TODO(assets)`; each doc ends with **Known gaps**.
3. Wiki-only pages (Components, Roadmap, Known-Issues, Coding-Conventions) live in the `Hydro.wiki` checkout next to this repo.

## Rules

The `hydro-engine-architecture` skill has the rules and the before-finishing checklist; follow `docs/` when they disagree with anything else, or ask. The ones most often missed: everything in `namespace Hydro` (no other, nested or anonymous namespaces), no `!` operator (`== false`, `== nullptr`), braces on every `if`/`else`/loop body, `m_PascalCase` members, `Init`/`Shutdown` for subsystems and RAII for resources, components are plain data, no Vulkan in new `Renderer/` headers, CRLF line endings.

## Working from a cloud session

- **Reading**: clone `https://github.com/joelnl74/Hydro` (public) and search there. It only has what is pushed; ask the user what is uncommitted, and stage those files from their computer instead.
- **Editing**: change the files in the user's folder on their computer (copy over only the files you edit, guarded by their modification time), not the clone.
- **Checking**: `scripts/check.sh [files...]` = g++ syntax check (`-Wall -Wextra`) + glslang for the shaders. It is not a build: say the change still needs an MSVC Debug build. A real Linux build is possible too (`scripts/Linux-GenProjects.sh`, `scripts/Build.sh`) when a Vulkan SDK (headers, loader, shaderc, glslc) and the X11 headers GLFW needs are available; Oxygen runs under Xvfb with a software driver (`VK_ICD_FILENAMES`, e.g. SwiftShader).
- **Docs**: edit `docs/`, then `python scripts/regen_wiki.py --wiki <path to Hydro.wiki>` (or `--check`). Update `docs/CodeMap.md` when files are added, renamed or deleted, and the wiki's Components / Roadmap / Known-Issues pages by hand.
- Ask for both folders (`Hydro` and `Hydro.wiki`) at the start of a task that changes docs.
