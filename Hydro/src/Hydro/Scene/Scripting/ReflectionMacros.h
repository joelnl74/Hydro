// Hydro/Scene/Scripting/ReflectionMacros.h
#pragma once

// These macros expand to nothing. They are markers for Tools/ReflectionGenerator/generate_reflection.py,
// which reads the headers and writes the scripts' registration (Oxygen/src/Generated/GeneratedScriptReflection.cpp).
//
// HY_SCRIPT()   above a class or struct that derives publicly from ScriptableEntity (`final` is fine).
//               It is registered under its class name, which is what scenes save.
//
// HY_PROPERTY(...) above a public field of such a class: shown in the inspector, saved with the scene and
// applied before OnCreate. Types: bool, int, float, glm::vec2, glm::vec3, glm::vec4, std::string, AssetHandle.
// One field per declaration. Arguments, in any order, all optional:
//     Range(min, max)       int / float / vector components: clamped when the field is left
//     Min(value), Max(value)
//     Tooltip("text")
//     DisplayName("Label")  instead of the field's name split at capitals
//     Color                 glm::vec3 / glm::vec4 edited with a colour picker
//     Asset(Texture2D)      required for an AssetHandle: the AssetType its picker lists
//     ReadOnly              shown, not editable
//     Hidden                saved and applied, not shown
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

#define HY_SCRIPT(...)
#define HY_PROPERTY(...)
#define HY_DATA(...)
#define HY_REFLECT(...)
