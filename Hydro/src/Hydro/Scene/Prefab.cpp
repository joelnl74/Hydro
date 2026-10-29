#include "hypch.h"
#include "Prefab.h"

namespace Hydro
{
	Ref<Prefab> Prefab::LoadUncached(const std::filesystem::path& path)
	{
		try
		{
			YAML::Node data = YAML::LoadFile(path.string());
			std::vector<YAML::Node> entities;
			if (data["Prefab"].IsDefined())
			{
				// Entities: the root and its subtree (Oct 29, 2026); Entity: one entity (older prefabs).
				if (auto list = data["Entities"]; list && list.IsSequence())
				{
					for (const YAML::Node& node : list)
					{
						entities.push_back(node);
					}
				}
				else if (auto single = data["Entity"]; single && single.IsMap())
				{
					entities.push_back(single);
				}
			}
			if (entities.empty())
			{
				HY_CORE_ERROR("Prefab: '{}' is not a Hydro prefab file", path.string());
				return nullptr;
			}

			// Not CreateRef: the constructor is private, so only this loader makes prefabs.
			Ref<Prefab> prefab(new Prefab());
			prefab->m_Name = data["Prefab"].as<std::string>("");
			prefab->m_Entities = std::move(entities);
			return prefab;
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("Prefab: failed to load '{}': {}", path.string(), e.what());
			return nullptr;
		}
	}
}
