---
name: "hydro-quick-edit"
description: "Use for small, contained changes in the Hydro engine or Oxygen editor (fix a warning, rename, add a field or inspector row, add an editor setting, tidy a function, update a doc row). Short rules and a checklist; switch to hydro-engine-architecture for anything bigger."
---

# Hydro quick edit

Use this for small changes that touch a few files and don't change how the engine is put together. The full rules are in the `hydro-engine-architecture` skill and the repository's `docs/` folder (the source of truth).

## Is it small?

**Small** (use this skill): fix a compiler warning; rename a variable or fix a misspelled name in one area; add a field to a component, spec or settings struct (with its save/load and inspector row); add a row to an editor settings page; add a menu item or button that calls an existing function; a comment or doc fix; a Known-Issues / Roadmap table row.

**Not small** (load `hydro-engine-architecture`, read the docs first, or hand it to Claude): a new subsystem, resource, render pass, component type or system; anything touching Vulkan object lifetime, the frame loop, startup/shutdown order or threading; moving files; changing a public engine API used in several places; anything across more than ~5 files. If unsure, treat it as not small.

## Rules that apply to every edit

1. **Namespace.** Everything is in `namespace Hydro`. No other, nested or anonymous namespaces. File-local helpers are `static` functions; a group of helpers is a class with static functions.
2. **No `!`.** Write `== false` for booleans and `== nullptr` for pointers.
3. **Always braces.** Every `if` / `else` / `else if` / `for` / `while` / `do` body is in `{ }`, each brace on its own line, even for one statement:

   ```cpp
   if (pixels == nullptr)
   {
       return;
   }
   ```

   Never `if (x) return;`. Only `case X: return Y;` in a `switch` may stay on one line.
4. **Names.** Types, methods and struct/spec fields `PascalCase`; members `m_PascalCase`; statics `s_PascalCase`; `enum class Name : type` (never `typedef enum class`). Lifecycle is `Init` / `Shutdown`; engine hooks start with `On`.
5. **Signatures.** One-argument constructors are `explicit`. Pass objects you only use as `T&` / `const T&`; a `Ref<T>` you don't keep as `const Ref<T>&`. Unused but required parameters get `[[maybe_unused]]`.
6. **Ownership.** `Scope<T>` for an object with one owner (`CreateScope`), `Ref<T>` only when shared (`CreateRef`). No new manual `Destroy()` methods.
7. **Includes.** Forward slashes. Engine headers from `src`: `#include "Hydro/Scene/Entity.h"` (never `../`); same-folder headers by bare name. Vendored libraries by their own name: `<entt.hpp>`, `<imgui.h>`, `<glm/glm.hpp>`. Headers have `#pragma once` and never include `hypch.h`.
8. **Checks.** `VK_CHECK_RESULT(...)` around Vulkan calls that return `VkResult`; `HY_CORE_ASSERT(condition, "message")` for debug checks.
9. **Layering.** Engine code never includes Oxygen code. Applications include `Hydro.h`, not `Platform/Vulkan` headers. Headers under `Renderer/` don't include Vulkan. Components are plain data (no virtual functions, no logic, no pointers to other entities).
10. **Format.** Tabs, braces on their own line, CRLF line endings; follow `.clang-format` / `.editorconfig`. Only reformat lines you change. No new compiler warnings (`/W4`).

## Recipes

**New component field** (e.g. `SpriteRendererComponent::SortOrder`):
1. Add it with a default in `Scene/Components/Components.h`.
2. Save and load it in `Scene/EntitySerializer.cpp` (load keeps the default when the key is missing).
3. Show it in `Oxygen/src/EditorWindows/InspectorWindow.cpp` as a typed input field (not a drag bar).
4. Add it to the wiki page `Components`.

**New editor setting** (Oxygen only):
1. Add the field with a default to the right struct in `Oxygen/src/EditorSettings.h`.
2. Write it in `ToYaml` and read it in `ReadSettings` (`EditorSettings.cpp`), using the existing `ReadFloat` / `ReadBool` / `ReadVec4` helpers.
3. Add a row on the matching page in `EditorWindows/EditorSettingsWindow.cpp` (`CheckboxRow`, `FloatRow`, `ColorRow`) and return `changed`.
4. Use it where it applies (`EditorLayer` reads `m_EditorSettings` every frame).

**Reading YAML:** check that a key exists (`if (auto child = node[key])` or `child.IsDefined()`) before calling `IsMap()`, `IsSequence()` or `as<T>()`. A missing key otherwise throws `YAML::InvalidNode` and crashes the editor.

**UI colours:** never hard-code; use `ImGui::GetStyle().Colors[...]` or `EditorStyle::GetHighlightColor()`, and make sure text is readable on the light theme (Glacier).

## Before you finish

- [ ] Every rule above holds in the lines you changed (no `!`, every `if`/loop body in `{ }`)
- [ ] Only the files the task needs were changed; line endings unchanged (CRLF)
- [ ] Nothing invented: every function, field and file you used exists (search before you call it)
- [ ] If a component, setting, spec or behaviour changed: the matching `docs/` page or wiki row is updated
- [ ] Say what you did not verify (you can't build or run the editor; the user does)
