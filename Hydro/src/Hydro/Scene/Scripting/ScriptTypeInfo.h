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

		// Names it was saved under before (HY_SCRIPT(FormerName("..."))): a scene that still says one binds this
		// script, and saving writes Name. Delete a former name once every scene and prefab has been saved.
		std::vector<std::string> FormerNames;

		std::type_index Type{
			typeid(void)
		};

		ScriptFactory Factory = nullptr;

		std::vector<ScriptProperty> Properties;
	};
}
