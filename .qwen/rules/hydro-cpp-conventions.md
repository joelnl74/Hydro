---
description: Hydro engine and Oxygen editor C++ conventions, applied when reading or editing source files.
paths:
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.inl"
  - "**/*.cpp"
---
# Hydro / Oxygen C++ rules

- Small, contained change: follow the `hydro-quick-edit` skill. Architecture work (new subsystem, resource, pass, component type or system; Vulkan lifetimes; frame or startup order): follow `hydro-engine-architecture` and read the matching `docs/` file first. The repository docs are the authority; if one is missing, ask.
- Everything lives in `namespace Hydro`: no other, nested or anonymous namespaces. File-local helpers are `static` functions; helper groups are classes with static functions. Only `std::hash<>` and `YAML::convert<>` specializations live elsewhere.
- No unary `!`: write `== false` for booleans and `== nullptr` for pointers.
- Names: PascalCase types, methods and struct/spec fields; `m_PascalCase` members; `s_PascalCase` statics; `enum class` (never `typedef enum class`). Subsystems use `Init` / `Shutdown`; layers `OnAttach` / `OnDetach`; engine hooks start with `On`. Never name an engine service `...System` (that word means an ECS function over a view).
- `explicit` one-argument constructors; `const T&` / `T&` for objects you only use; `const Ref<T>&` for a shared pointer you don't keep; `[[maybe_unused]]` for required-but-unused parameters.
- Ownership: `Scope<T>` for single-owner GPU objects, `Ref<T>` only when shared; RAII destructors, no new `Destroy()` methods.
- Resources take a `Specification` (and optional `Properties`) struct; no Vulkan types in specs or headers under `Renderer/`. Components are plain data.
- `VK_CHECK_RESULT(...)` for Vulkan results; `HY_CORE_ASSERT(...)` for debug checks.
- Includes: forward slashes; engine headers as `"Hydro/..."` from `src` (no `../`); vendored headers by name (`<entt.hpp>`, `<imgui.h>`); `#pragma once`; headers never include `hypch.h`.
- YAML: check a key exists (`if (auto child = node[key])` / `IsDefined()`) before `IsMap()`, `IsSequence()` or `as<T>()`.
- Format with `.clang-format` (tabs, Allman braces), keep CRLF, add no `/W4` warnings, change only what the task needs.
- Don't invent files, symbols or build results; you can't build here, so say what is unverified.
