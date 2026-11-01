#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/EntityRef.h"

#include <glm/glm.hpp>

#include <cfloat>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace Hydro
{
	class Entity;
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
		Asset, // an AssetHandle (0 = none); ScriptProperty::AssetFilter says which kind of asset
		RowRef, // a DataRowRef (a data table's row); ScriptProperty::DataTypeFilter says the row type
		EntityRef // an EntityRef or a ComponentRef<T> (SC4): an entity's UUID; AcceptsEntity / TargetType say what it needs
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
			AssetHandle,
			DataRowRef,
			EntityRef>;

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
		// The saved key: the field's name. Rename it only with a former name (below).
		std::string Name;
		// Keys it was saved under before (HY_PROPERTY(FormerName("..."))): read when the scene has no value under
		// Name; saving writes Name. Delete a former name once every scene and prefab has been saved.
		std::vector<std::string> FormerNames;
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
		std::string DataTypeFilter;   // Asset of a Definition / DataTable: the HY_DATA type it holds; RowRef: the row type
		bool ReadOnly = false;        // shown, not editable (runtime state a script writes)
		bool HideInInspector = false; // saved and applied, not shown
		// Asset, RowRef, EntityRef: must be set (HY_PROPERTY(Required)). The inspector shows an empty or broken one in
		// the warning colour; project validation (SC5) reports it. Code still checks Get() for nullptr.
		bool Required = false;
		// EntityRef of a ComponentRef<T>: the name of T for the inspector ("needs a DialogueScript"), and whether an
		// entity has a T (outside Play a bound script counts). Unset for a plain EntityRef: any entity.
		std::string TargetType;
		std::function<bool(const Entity&)> AcceptsEntity;

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
