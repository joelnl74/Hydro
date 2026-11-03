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

		// Old names: a scene that still says one binds this script, and saving writes Name. Delete once every file is saved.
		std::vector<std::string> FormerNames;

		std::type_index Type{
			typeid(void)
		};

		ScriptFactory Factory = nullptr;

		std::vector<ScriptProperty> Properties;

		// The class overrides OnDrawGizmos, so the Editor view asks it to draw.
		bool DrawsGizmos = false;
	};
}
