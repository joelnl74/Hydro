// Hydro/Scene/Scripting/ReflectionMacros.h
#pragma once

// Markers that expand to nothing; Tools/ReflectionGenerator/generate_reflection.py reads them and writes the
// registration into Game/src/Generated. Examples and details: docs/Scripting.md and docs/SceneAndECS.md.
//
// HY_SCRIPT(FormerName("Old", ...))   a class deriving publicly from ScriptableEntity, registered by class name
// HY_COMPONENT(FormerName(...))       a plain-data game ECS component (ComponentRegistry)
// HY_DATA(Definition, Table)          a data type; also HY_DATA() for nested helpers, DisplayField(Field), FormerName
// HY_ENUM()                           an enum class saved by enumerator name (reordering is safe, renaming is not)
// HY_PROPERTY(...)                    a public field: shown, saved, applied before OnCreate. One field per declaration.
//   Types: bool, int, float, glm::vec2/3/4, std::string, AssetHandle, DataRowRef, EntityRef, ComponentRef<T>,
//   HY_ENUM enums, HY_DATA structs and std::vector of those (no nested lists).
//   Arguments: Range(min, max), Min(v), Max(v), Tooltip("text"), DisplayName("Label"), Color, Asset(Type),
//   Asset(Definition, T), Asset(DataTable, T), DataRow(T), ReadOnly, Hidden, FormerName("Old", ...), Required.
//
// FormerName keeps files saved under an old name loading; delete it once every file has been saved again.

#define HY_SCRIPT(...)
#define HY_PROPERTY(...)
#define HY_DATA(...)
#define HY_COMPONENT(...)
#define HY_ENUM(...)
#define HY_REFLECT(...)
