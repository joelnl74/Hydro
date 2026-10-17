#include "hypch.h"
#include "SceneSerializer.h"
#include "EntitySerializer.h"

namespace Hydro
{
	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		// Collect first so the file keeps creation order (entt iterates newest first).
		std::vector<entt::entity> entities;
		m_Scene->m_Registry.each([&](auto entityHandle)
		{
			// Only "real" entities (made through CreateEntity) are saved. Raw registry entities
			// without an ID, like the editor's demo galaxy stars, are runtime-only.
			if (m_Scene->m_Registry.has<IDComponent>(entityHandle))
				entities.push_back(entityHandle);
		});

		for (auto it = entities.rbegin(); it != entities.rend(); ++it)
			EntitySerializer::Serialize(out, Entity{ *it, m_Scene.get() });

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::error_code ec;
		if (filepath.has_parent_path())
			std::filesystem::create_directories(filepath.parent_path(), ec);

		std::ofstream fout(filepath);
		if (fout.is_open() == false)
		{
			HY_CORE_ERROR("SceneSerializer: could not open '{}' for writing", filepath.string());
			return false;
		}

		fout << out.c_str();
		HY_CORE_INFO("Saved scene '{}' ({} entities) to {}", m_Scene->GetName(), entities.size(), filepath.string());
		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("SceneSerializer: failed to load '{}': {}", filepath.string(), e.what());
			return false;
		}

		if (data["Scene"].IsDefined() == false)
		{
			HY_CORE_ERROR("SceneSerializer: '{}' is not a Hydro scene file", filepath.string());
			return false;
		}

		m_Scene->SetName(data["Scene"].as<std::string>());

		size_t count = 0;
		if (auto entities = data["Entities"])
		{
			try
			{
				for (auto entityNode : entities)
				{
					EntitySerializer::Deserialize(entityNode, *m_Scene, true);
					count++;
				}
			}
			catch (const YAML::Exception& e)
			{
				HY_CORE_ERROR("SceneSerializer: bad entity data in '{}': {}", filepath.string(), e.what());
				return false;
			}
		}

		HY_CORE_INFO("Loaded scene '{}' ({} entities) from {}", m_Scene->GetName(), count, filepath.string());
		return true;
	}
}
