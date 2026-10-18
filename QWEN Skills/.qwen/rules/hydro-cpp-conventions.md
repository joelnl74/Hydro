---
description: Apply Hydro Vulkan engine and Oxygen editor C++ code conventions when working with source files.
paths:
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.hxx"
  - "**/*.inl"
  - "**/*.ixx"
  - "**/*.c"
  - "**/*.cc"
  - "**/*.cpp"
  - "**/*.cxx"
---
# Hydro / Oxygen C++ guardrails

- Follow the `hydro-engine-architecture` skill for architecture changes, reviews, and code generation. Read the matching repository `docs/` files first; the repository documentation is the authority. If a referenced file is missing, ask rather than assume its contents.
- Match nearby formatting and naming. Types/methods use PascalCase; member fields use `m_`; static state uses `s_`.
- Do not use unary logical-not (`!`). Express boolean checks as `== false` and pointer checks as `== nullptr`.
- Distinguish subsystems, parts, layers, ECS systems, and resources. Do not invent another lifecycle: subsystems/parts use `Init` and `Shutdown`, layers use `OnAttach` and `OnDetach`, GPU resources use RAII and destruction after GPU completion.
- Use `Ref<T>` / `Scope<T>` and `CreateRef` / `CreateScope` consistently. New resources take a `Specification` and optional `Properties` parameter object; no post-creation setters for resource identity.
- Add no `...System` engine services: `System` means an ECS update function over `registry.view<...>()`. Components are data-only.
- Wrap Vulkan results with `VK_CHECK_RESULT(...)`; use `HY_CORE_ASSERT(...)` for debug invariants. Keep Vulkan types out of engine-level specifications.
- Use forward slashes in `#include` paths; use `#pragma once` in headers; do not include `hypch.h` in headers. Preserve existing CRLF line endings.
- Do not invent source symbols or claim a build passed unless you ran it. Keep changes scoped; report any verified architectural gap.
