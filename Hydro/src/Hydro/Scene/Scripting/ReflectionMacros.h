// Hydro/Scene/Scripting/ReflectionMacros.h
#pragma once

// These macros expand to nothing. They are markers for Tools/ReflectionGenerator/generate_reflection.py,
// which reads the headers and writes the registration: scripts into Game/src/Generated/GeneratedScriptReflection.cpp,
// data types and enums into Game/src/Generated/GeneratedDataReflection.cpp.
//
// HY_SCRIPT()   above a class or struct that derives publicly from ScriptableEntity (`final` is fine).
//               It is registered under its class name, which is what scenes save. Its only argument:
//     FormerName("OldName", ...)  names scenes saved before the class was renamed (see Former names below)
//
// HY_PROPERTY(...) above a public field of such a class: shown in the inspector, saved with the scene and
// applied before OnCreate. Types: bool, int, float, glm::vec2, glm::vec3, glm::vec4, std::string, AssetHandle,
// DataRowRef (a data table's row).
// One field per declaration. Arguments, in any order, all optional:
//     Range(min, max)       int / float / vector components: clamped when the field is left
//     Min(value), Max(value)
//     Tooltip("text")
//     DisplayName("Label")  instead of the field's name split at capitals
//     Color                 glm::vec3 / glm::vec4 edited with a colour picker
//     Asset(Texture2D)      required for an AssetHandle: the AssetType its picker lists
//     Asset(Definition, DialogueDefinition), Asset(DataTable, ItemDefinition)
//                           a definition / data table holding that HY_DATA type (read it with Data::Get / GetTable)
//     DataRow(ItemDefinition) required for a DataRowRef: the row type (read it with Data::GetRow)
//     ReadOnly              shown, not editable
//     Hidden                saved and applied, not shown
//     FormerName("Old", ...) keys files saved before the field was renamed (see Former names below)
//
//     HY_SCRIPT()
//     class LanternScript : public ScriptableEntity
//     {
//     public:
//         HY_PROPERTY(Range(0, 10), Tooltip("Flicker strength"))
//         float Flicker = 2.0f;
//         HY_PROPERTY(Color)
//         glm::vec3 Glow{ 1.0f, 0.7f, 0.3f };
//         HY_PROPERTY(Asset(Texture2D))
//         AssetHandle LitSprite = 0;
//     };

// HY_DATA(...) above a struct of HY_PROPERTY fields: a data type (Hydro/Data/DataTypeRegistry.h), saved in
// definitions (.hydef, one record) and data tables (.hytable, rows by ID). Registered under its struct name, which
// is what files save. Data types don't derive from anything. Exactly these arguments, nothing else:
//     HY_DATA()                         a helper type, only used inside other data types (DialogueLine)
//     HY_DATA(Definition)               can be created as a definition: Create > Definition
//     HY_DATA(Table)                    can be created as a data table: Create > Data Table
//     HY_DATA(Definition, Table)        both
//     DisplayField(Field)               (with Table) the std::string field row lists and pickers show; default:
//                                       a std::string field called Name, else the row ID. Editor-only
//     FormerName("OldName", ...)        type names files saved before the struct was renamed
// Field types: the script types above, plus DataRowRef (a table row), an HY_ENUM enum class (saved by name), another
// HY_DATA struct (a nested map) and std::vector<T> of any of these except a list. Arguments as for scripts (on a list
// they apply to its elements), including Asset(Definition, T) / Asset(DataTable, T) and DataRow(T).
//
//     HY_DATA()
//     struct DialogueLine
//     {
//         HY_PROPERTY(Tooltip("Who says it; empty = the dialogue's speaker"))
//         std::string Speaker;
//         HY_PROPERTY()
//         std::string Text;
//     };
//
//     HY_DATA(Definition)
//     struct DialogueDefinition
//     {
//         HY_PROPERTY()
//         std::string Speaker;
//         HY_PROPERTY()
//         std::vector<DialogueLine> Lines;
//     };
//
// Former names: renaming a script class, a property, a data type or a data field changes what files save. Give the
// new name FormerName("Old") and files saved under the old one still load: the binding, value or file is read into
// the renamed one, the load is logged (scripts) or reported (data), and saving writes the new name. Once every scene,
// prefab and data file has been saved, delete the FormerName; the log no longer mentions the old name by then. A
// former name may not be a current name or another's former name (the generator fails).
//
//     HY_SCRIPT(FormerName("MvpDirectorScript"))
//     class FieldDirectorScript : public ScriptableEntity
//     {
//     public:
//         HY_PROPERTY(Range(0, 20), FormerName("InteractRange"))
//         float ReachMetres = 1.7f;
//     };
//
// HY_ENUM() above an enum class used by data fields. Saved by the enumerator's name, so reordering or renumbering is
// safe and renaming is not (files keep the old name and validation reports it).
//
//     HY_ENUM()
//     enum class ItemKind : uint8_t { Consumable, Equipment, KeyItem };

#define HY_SCRIPT(...)
#define HY_PROPERTY(...)
#define HY_DATA(...)
#define HY_ENUM(...)
#define HY_REFLECT(...)
