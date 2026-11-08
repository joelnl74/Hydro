# Qwen Code + local Ollama: Hydro / Oxygen setup

These files give Qwen Code (with a local Ollama model) the same rules Claude uses in this repository, sized for small local models. They don't change Ollama, a model or Claude.

## What's here

| File | What it is | When it loads |
| --- | --- | --- |
| `QWEN.md` | Project instructions: what Qwen is used for (lightweight tasks) and which skill to pick | Always |
| `.qwen/rules/hydro-cpp-conventions.md` | The C++ rules in short form (namespace, naming, no `!`, ownership, includes, YAML, format) | When Qwen reads or edits `.h` / `.hpp` / `.inl` / `.cpp` files |
| `.qwen/skills/hydro-quick-edit/SKILL.md` | Small-task skill: what counts as small, the rules, recipes (component field, editor setting, YAML, UI colours), a checklist | When invoked (`/hydro-quick-edit`) or picked by the model |
| `.qwen/skills/hydro-engine-architecture/SKILL.md` | The full architecture skill, the same text as Claude's `hydro-engine-architecture` skill | When invoked (`/hydro-engine-architecture`) or picked by the model |

The full skill is large (about 27 KB). On small models prefer `hydro-quick-edit` and only load the full skill for architecture questions.

## Install

Qwen Code looks for `.qwen/` and `QWEN.md` in the folder it starts in, so copy them next to `Hydro.sln` (this folder keeps the master copy):

```bat
cd /d C:\Users\joeln\Hydro
xcopy /E /I /Y "QWEN Skills\.qwen" .qwen
copy /Y "QWEN Skills\QWEN.md" QWEN.md
qwen
```

If `.qwen\settings.json` already exists, keep it; the copy only adds `rules\` and `skills\`. Instead of copying, you can point Qwen at this folder in `.qwen/settings.json`:

```json
{
  "skills": {
    "directories": ["C:/Users/joeln/Hydro/QWEN Skills/.qwen/skills"]
  }
}
```

Use either the copy or the setting, not both (the skills would be registered twice).

In Qwen Code, `/skills` should list `hydro-quick-edit` and `hydro-engine-architecture`. Quick test:

```
Use hydro-quick-edit: add an int SortOrder field (default 0) to SpriteRendererComponent, save/load it and show it in the inspector. Show the diff, don't build.
```

## Keeping it in sync

The full skill is a copy of the Claude skill. When the Claude skill or `docs/` change, update `.qwen/skills/hydro-engine-architecture/SKILL.md` with the same text and adjust the short rules and the quick-edit skill if a rule changed. Last synced: October 19, 2026 (agreed changes A1–A12, tooling, editor look and settings, YAML reading rule).

## Notes

- Skills and rules are instructions read at run time; they don't retrain the model.
- Small local models still miss rules sometimes: review the diff, format with `.clang-format`, and build with `/W4` before committing.
- References: Qwen Code skills (https://qwenlm.github.io/qwen-code-docs/en/users/features/skills/), rules (https://qwenlm.github.io/qwen-code-docs/en/users/features/rules/), settings (https://qwenlm.github.io/qwen-code-docs/en/users/configuration/settings/).
