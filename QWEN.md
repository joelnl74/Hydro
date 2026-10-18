# Hydro and Oxygen: Qwen (local model) instructions

This repository is the Hydro Vulkan game engine (static library `Hydro`) and its editor `Oxygen`. The `docs/` folder is the source of truth; the wiki (`Hydro.wiki`) mirrors it.

You are used for **lightweight tasks**: small, contained edits, warning fixes, renames, adding a field or an editor setting, explaining code, reviewing a short diff, updating a doc row. Bigger work (new subsystems, resources, render passes, component types or systems, Vulkan lifetimes, frame/startup order, multi-file refactors) is done with Claude; for those, describe the plan and stop instead of editing.

- Small edits: use the `hydro-quick-edit` skill.
- Architecture questions or reviews: use the `hydro-engine-architecture` skill and read the matching `docs/` file first.
- C++ rules (namespace, naming, no `!`, ownership, includes) are in `.qwen/rules/hydro-cpp-conventions.md` and load automatically for source files.
- Keep CRLF line endings and the existing formatting; change only what the task needs.
- Don't invent files, APIs, build results or decisions. Search before you call a function. You can't build the project: say what is unverified.
