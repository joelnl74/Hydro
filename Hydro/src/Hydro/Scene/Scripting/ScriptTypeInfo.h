#pragma once
#include "ScriptProperty.h"

#include <string>
#include <typeindex>
#include <vector>

namespace Hydro
{
	class ScriptableEntity;

	using ScriptFactory =
		ScriptableEntity* (*)();

	struct ScriptTypeInfo
	{
		std::string Name;

		std::type_index Type{
			typeid(void)
		};

		ScriptFactory Factory = nullptr;

		std::vector<ScriptProperty> Properties;
	};
}
