#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace Hydro
{
	class ScriptableEntity;

	enum class ScriptPropertyType
	{
		Bool,
		Int,
		Float
	};

	using ScriptPropertyValue =
		std::variant<
			bool,
			int,
			float>;

	using ScriptPropertyMap =
		std::unordered_map<
			std::string,
			ScriptPropertyValue>;

	struct ScriptProperty
	{
		std::string Name;
		std::string DisplayName;

		ScriptPropertyType Type =
			ScriptPropertyType::Float;

		ScriptPropertyValue DefaultValue =
			0.0f;

		// Takes the stored editor/scene value and
		// pushes it into the actual C++ script object.
		std::function<void(
			ScriptableEntity&,
			const ScriptPropertyValue&)>
			Apply;
	};
}
