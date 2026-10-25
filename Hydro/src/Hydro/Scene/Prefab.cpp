#include "hypch.h"
#include "Prefab.h"

namespace Hydro
{
	Ref<Prefab> Prefab::LoadUncached(const std::filesystem::path& path)
	{
		try
		{
			YAML::Node data = YAML::LoadFile(path.string());
			if (data["Prefab"].IsDefined() == false || data["Entity"].IsDefined() == false)
			{
				HY_CORE_ERROR("Prefab: '{}' is not a Hydro prefab file", path.string());
				return nullptr;
			}

			// Not CreateRef: the constructor is private, so only this loader makes prefabs.
			Ref<Prefab> prefab(new Prefab());
			prefab->m_Name = data["Prefab"].as<std::string>("");
			prefab->m_Entity = data["Entity"];
			return prefab;
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("Prefab: failed to load '{}': {}", path.string(), e.what());
			return nullptr;
		}
	}
}
