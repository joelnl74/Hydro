# Qwen Code + local Ollama: Hydro / Oxygen project setup

This archive is a **project overlay**. It does not modify Ollama, install a model, or change Claude Code.

## Install inside your Visual Studio solution

1. Extract this archive into the directory containing the Hydro/Oxygen `.sln` file, keeping the `.qwen` directory structure.
2. If `QWEN.md` does not already exist, rename `QWEN.hydro-example.md` to `QWEN.md`. If it exists, **merge the example** rather than overwriting the original.
3. Keep the project's real `docs/` documentation available. The skill refers to `docs/Architecture.md`, `docs/Subsystems.md`, `docs/ResourceCreation.md`, and `docs/SceneAndECS.md`, as well as `Hydro.wiki`. Those are **not bundled here**.
4. In Windows Command Prompt, use `cd /d "C:\path\to\your\solution"` and then run `qwen`, configured to your **local** Ollama model.
5. In Qwen Code type `/skills` to verify `hydro-engine-architecture` is discoverable, or run it directly using `/hydro-engine-architecture`.
6. Test: `Use hydro-engine-architecture to review this VulkanImage class for ownership and style violations. Read relevant docs first. Do not edit files.`

## Included files

- `.qwen/skills/hydro-engine-architecture/SKILL.md` -- verbatim copy of your Claude skill, compatible with Qwen's Agent Skills format.
- `.qwen/rules/hydro-cpp-conventions.md` -- concise path-gated C++ coding rules that activate when Qwen reads/edits matching source files.
- `QWEN.hydro-example.md` -- optional always-loaded project instructions. Rename/merge into `QWEN.md`.

## Alternative: share the original Claude skill without copies

Qwen Code also supports `skills.directories` in `.qwen/settings.json`. If the skill already lives in `<solution>/.claude/skills/hydro-engine-architecture/SKILL.md`, you can point Qwen at that **existing parent folder**, using an absolute Windows path in this setting instead of installing the duplicate `.qwen/skills/...` copy:

```json
{
  "skills": {
    "directories": ["C:/path/to/solution/.claude/skills"]
  }
}
```

Do **not** overwrite an existing `.qwen/settings.json`; merge the `skills.directories` entry. Choose either the copied skill or the shared skill path to avoid duplicate registrations.

## Notes

- A skill supplies instructions at inference time; it does not retrain the model.
- The full skill only gets loaded when invoked; rules have separate conditional activation. This can save context compared to copying everything into `QWEN.md`.
- Small local models may still fail to follow a rule: review diffs, use `.clang-format` if present, and compile/run tests.
- These files have been structurally validated; they have not been exercised against your actual Hydro source repository, which was not provided.

Official Qwen Code references:
- https://qwenlm.github.io/qwen-code-docs/en/users/features/skills/
- https://qwenlm.github.io/qwen-code-docs/en/users/features/rules/
- https://qwenlm.github.io/qwen-code-docs/en/users/configuration/settings/
