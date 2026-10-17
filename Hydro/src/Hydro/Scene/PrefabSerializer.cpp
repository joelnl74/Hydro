#include "hypch.h"
#include "PrefabSerializer.h"
#include "EntitySerializer.h"

namespace Hydro
{
	bool PrefabSerializer::Save(Entity entity, const std::filesystem::path& filepath)
	{
		if ((bool)entity == false || entity.HasComponent<IDComponent>() == false)
		{
			HY_CORE_ERROR("PrefabSerializer: can only save entities created with Scene::CreateEntity");
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
		out << YAML::Key << "Entity" << YAML::Value;
		EntitySerializer::Serialize(out, entity);
		out << YAML::EndMap;

		std::error_code ec;
		if (filepath.has_parent_path())
			std::filesystem::create_directories(filepath.parent_path(), ec);

		std::ofstream fout(filepath);
		if (fout.is_open() == false)
		{
			HY_CORE_ERROR("PrefabSerializer: could not open '{}' for writing", filepath.string());
			return false;
		}

		fout << out.c_str();
		HY_CORE_INFO("Saved prefab '{}' to {}", entity.GetName(), filepath.string());
		return true;
	}

	Entity PrefabSerializer::Instantiate(Scene& scene, const std::filesystem::path& filepath)
	{
		Entity entity = Load(scene, filepath);
		if (entity)
			HY_CORE_INFO("Instantiated prefab '{}' from {}", entity.GetName(), filepath.string());

		return entity;
	}

	Entity PrefabSerializer::Load(Scene& scene, const std::filesystem::path& filepath)
	{
		try
		{
			YAML::Node data = YAML::LoadFile(filepath.string());

			if (data["Prefab"].IsDefined() == false || data["Entity"].IsDefined() == false)
			{
				HY_CORE_ERROR("PrefabSerializer: '{}' is not a Hydro prefab file", filepath.string());
				return {};
			}

			return EntitySerializer::Deserialize(data["Entity"], scene, false);
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("PrefabSerializer: failed to load '{}': {}", filepath.string(), e.what());
			return {};
		}
	}
}
