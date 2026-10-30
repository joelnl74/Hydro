# Hydro: notes for Claude

Vulkan game engine (`Hydro`, static library), the game's code (`Game`, static library: the scripts), its editor (`Oxygen`) and the standalone player (`Runtime`). Windows + MSVC + premake; C++17.

## Find things cheaply

1. **`docs/CodeMap.md` first**: every file in one line, and which files each kind of change touches. Then open only the section of the full doc you need (`docs/Architecture.md`, `Subsystems.md`, `ResourceCreation.md`, `SceneAndECS.md`); don't read them end to end.
2. Search, don't guess: `grep -rn "Name" Hydro/src Oxygen/src`. Unfinished work is tagged `TODO(assets)`; each doc ends with **Known gaps**.
3. Wiki-only pages (Components, Roadmap, Known-Issues, Coding-Conventions) live in the `Hydro.wiki` checkout next to this repo.

## Rules

The `hydro-engine-architecture` skill has the rules and the before-finishing checklist; follow `docs/` when they disagree with anything else, or ask. The ones most often missed: everything in `namespace Hydro` (no other, nested or anonymous namespaces), no `!` operator (`== false`, `== nullptr`), braces on every `if`/`else`/loop body, `m_PascalCase` members, `Init`/`Shutdown` for subsystems and RAII for resources, components are plain data, no Vulkan in new `Renderer/` headers, CRLF line endings.

## Test bed

The gameplay test bed is the field scenes `Oxygen/project/Assets/Scenes/ShrineGlade.hyscene` and `FarmVillage.hyscene` (plus `Battle.hyscene`; the old `StatueVillage` scene was deleted, see `docs/TestBed.md`). When a new or drastic system lands (physics, audio, tilemaps, UI...), add it to those scenes as a gameplay element (colliders on buildings and the player, trigger zones, sound sources), not as a separate demo area, add a row to the Features table in `docs/TestBed.md`, and say in the summary what to check by pressing Play.

Runtime UI work (canvases, text, buttons, focus, pointer, menus) is checked in `Oxygen/project/Assets/Scenes/Battle.hyscene`, which already has a lot of UI worth making functional; keep the Shrine Glade free of UI test entities (it is the visual look's showcase). The game's own UI (battle HUD and menus, the field's inventory, dialogue and prompt) is built into the three scenes by `scripts/ui/build_game_ui.py` (art by `scripts/ui/make_ui_art.py`; check it offline with `scripts/ui/preview_ui.py`): change the builder and rerun it rather than hand-editing those entities.

3D renderer work (meshes, lighting, shadows, post-processing) is checked in `Oxygen/project/Assets/Scenes/ShrineGlade.hyscene` instead: a hillside shrine mixing meshes and upright sprites, built by `scripts/shrine_glade/build.py` (see `docs/TestBed.md`, "Shrine Glade"). **That builder (and `scripts/farm_village/`) is not in the repository or the user's folder right now**; until it is restored, edit the scenes in the editor and say so.

## Working from a cloud session

- **Reading**: clone `https://github.com/joelnl74/Hydro` (public) and search there. It only has what is pushed; ask the user what is uncommitted, and stage those files from their computer instead.
- **Editing**: change the files in the user's folder on their computer (copy over only the files you edit, guarded by their modification time), not the clone.
- **Checking**: `scripts/check.sh [files...]` = g++ syntax check (`-Wall -Wextra`) + glslang for the shaders. It is not a build: say the change still needs an MSVC Debug build.
- **Building**: `scripts\Build.bat [Debug|Release|Dist] [--project Hydro|Oxygen] [--rebuild] [--no-generate] [--no-pause]` regenerates the solution, builds with MSBuild and writes `bin\logs\Build-<Config>.log` / `-errors.log` / `-warnings.log`; exit code 0 = built, 1 = build failed, 2 = setup problem. On the user's computer, stage the errors log to read it.
- **Packaging**: `scripts\Package.bat [Release|Distribution] [--project <file.hyproj>] [--no-build] [--no-pause]` builds `Runtime` and fills `Builds\<Project>-<Kind>\` (`<Project>.exe`, `shaderc_shared.dll`, `assets\shaders`, `Project\<Project>.hyproj` + its asset folder); Oxygen's **File > Build...** runs it. Check a package by starting its exe: it plays the start scene; logs in its `logs\` folder.
- **CI**: `.github/workflows/build.yml` builds Debug and Release with `Build.bat` on GitHub-hosted Windows for every push (plus the reflection `--check` and a wiki-mirror warning). Read results with `gh run list -R joelnl74/Hydro` and `gh run view <id> --log-failed`; the run summary lists errors and warnings. CI only sees pushed code.
- **Pushing**: never commit or push without an explicit yes. A single step the user can check: leave it uncommitted for them to review and push. Work spanning several steps at once: ask before pushing it to a `claude/<topic>` scratch branch (for CI), unless the user's prompt already allows it. Never push to `main` or the user's own branches.
- **Docs**: edit `docs/`, then `python scripts/regen_wiki.py --wiki <path to Hydro.wiki>` (or `--check`). Update `docs/CodeMap.md` when files are added, renamed or deleted, and the wiki's Components / Roadmap / Known-Issues pages by hand.
- Ask for both folders (`Hydro` and `Hydro.wiki`) at the start of a task that changes docs.
