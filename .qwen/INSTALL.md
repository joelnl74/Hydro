# Qwen Code + local Ollama: Hydro / Oxygen setup

These files give Qwen Code (with a local Ollama model, `qwen3.6-64k`) the rules Claude uses in this repository, sized for a local model. In this repository Qwen reads, explains and makes small edits; engine changes are handed to a frontier model. Game scripting is set up in Eonlore (`..\Eonlore\.qwen\INSTALL.md`).

## What's here

| File | What it is | When it loads |
| --- | --- | --- |
| `QWEN.md` (repository root) | Qwen's role here, where things are, how to check, when to hand off | Always |
| `.qwen/rules/hydro-cpp-conventions.md` | The C++ rules in short form | When Qwen reads or edits `.h` / `.hpp` / `.inl` / `.cpp` files |
| `.qwen/skills/hydro-quick-edit/SKILL.md` | Small edits: what counts as small, recipes, checklist | On demand |
| `.qwen/skills/hydro-engine-architecture/SKILL.md` | The full architecture rules, the same text as Claude's skill (about 23 KB) | On demand: only to explain or review architecture |
| `%USERPROFILE%\.qwen\skills\hydro-handoff\SKILL.md` | The brief for a frontier model (shared with Eonlore) | On demand |

Qwen Code reads `QWEN.md` from the folder it starts in, so start it in the repository root. Project rules and skills load only once the folder is trusted.

## Start

```powershell
cd C:\Users\joeln\Hydro
qwen
```

`/skills` should list `hydro-quick-edit`, `hydro-engine-architecture` and `hydro-handoff`. Quick tests:

```
Explain how a script's OnTriggerEnter gets called, from the physics step to the script. Don't edit.
```
```
Add a post-processing pass for bloom.
```
The second should end in a hand-off brief, not an edit.

## Keeping it in sync

`hydro-engine-architecture` is a copy of the Claude skill. When the Claude skill or `docs/` change, copy its text here (keep this file's frontmatter) and adjust `hydro-quick-edit` and the rules file if a rule changed. Last synced: November 8, 2026.

## Notes

- Skills and rules are instructions read at run time; they don't retrain the model.
- Local models still miss rules: review the diff, run the linter and build with `/W4` before committing.
