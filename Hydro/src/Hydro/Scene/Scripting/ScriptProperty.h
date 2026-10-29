#pragma once

#include "Hydro/Asset/Asset.h"

#include <glm/glm.hpp>

#include <cfloat>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace Hydro
{
	class ScriptableEntity;

	// The kinds of value a script can expose with HY_PROPERTY. The order matches ScriptPropertyValue's
	// alternatives, so (size_t)type == value.index().
	enum class ScriptPropertyType : uint8_t
	{
		Bool,
		Int,
		Float,
		Vec2,
		Vec3,
		Vec4,
		String,
		Asset // an AssetHandle (0 = none); ScriptProperty::AssetFilter says which kind of asset
	};

	using ScriptPropertyValue =
		std::variant<
			bool,
			int,
			float,
			glm::vec2,
			glm::vec3,
			glm::vec4,
			std::string,
			AssetHandle>;

	using ScriptPropertyMap =
		std::unordered_map<
			std::string,
			ScriptPropertyValue>;

	const char* ScriptPropertyTypeToString(ScriptPropertyType type);

	// Whether `value` holds the alternative `type` stands for.
	inline bool ScriptPropertyHasType(const ScriptPropertyValue& value, ScriptPropertyType type)
	{
		return value.index() == (size_t)type;
	}

	struct ScriptProperty
	{
		// The saved key: the field's name. Keep it stable once scenes use it.
		std::string Name;
		// The inspector's label: the name split at capitals ("MoveSpeed" -> "Move Speed"), or DisplayName("...").
		std::string DisplayName;

		ScriptPropertyType Type =
			ScriptPropertyType::Float;

		ScriptPropertyValue DefaultValue =
			0.0f;

		// How the inspector shows it: HY_PROPERTY(...) arguments, or the ScriptRegistration modifiers.
		std::string Tooltip;
		float Min = -FLT_MAX; // Int, Float and each vector component: clamped when the field is left
		float Max = FLT_MAX;
		bool IsColor = false;         // Vec3 / Vec4 edited as a colour
		AssetType AssetFilter = AssetType::None; // Asset: which assets the picker lists
		bool ReadOnly = false;        // shown, not editable (runtime state a script writes)
		bool HideInInspector = false; // saved and applied, not shown

		// Takes the stored editor/scene value and
		// pushes it into the actual C++ script object.
		std::function<void(
			ScriptableEntity&,
			const ScriptPropertyValue&)>
			Apply;

		// Reads the field back from a running script, so the inspector can show what the script has now (#146).
		std::function<ScriptPropertyValue(
			const ScriptableEntity&)>
			Read;
	};
}
