# Hydro and Oxygen: Qwen (local model) instructions

This repository is the Hydro Vulkan game engine (static library `Hydro`), its editor `Oxygen`, the player `Runtime` and the small `Sample` game. `docs/` is the source of truth; the wiki (`..\Hydro.wiki`) mirrors it. The real game, Eonlore, is its own repository (`..\Eonlore`): **game scripting happens there**, with its own Qwen setup.

## Your job here: read, explain, small edits

- **Yes:** explain engine code and how a feature works; answer "which API does X" for game scripting; review a short diff; fix a warning, a typo or a name in one place; add a field with its save/load and inspector row (`hydro-quick-edit`); scripts in `Sample/Game`.
- **No, hand off** (`hydro-handoff` skill, a brief for a frontier model): new subsystems, resources, render passes, components or systems; Vulkan lifetimes; frame, startup or shutdown order; physics, UI or scene internals; the reflection generator; anything across more than ~5 files. Write the brief, don't edit.

## How to work

- Find files with `docs/CodeMap.md` first (one line per file), then open only the section of `docs/*.md` you need. Game scripting API: `docs/Scripting.md`.
- Search before you call a function; never invent files, APIs or build results.
- Skills: `hydro-quick-edit` (small edits, short rules), `hydro-engine-architecture` (the full rules: large, load only to explain or review architecture), `hydro-handoff`. C++ rules load by themselves for source files.
- Check: `py scripts\lint_conventions.py <files>`; build: `cmd /c "scripts\Build.bat Debug --no-pause"` (errors in `bin\logs\Build-Debug-errors.log`). Say what you didn't verify.
- Never commit or push. Keep CRLF, tabs and the existing formatting; change only what the task needs.
