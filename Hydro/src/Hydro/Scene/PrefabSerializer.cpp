#include "hypch.h"
#include "PrefabSerializer.h"
#include "EntitySerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Scene/Prefab.h"
#include "Hydro/Scene/Scene.h"

#include <unordered_map>
#include <vector>

namespace Hydro
{
	bool PrefabSerializer::Save(Entity entity, const std::filesystem::path& filepath)
	{
		if ((bool)entity == false || entity.HasComponent<IDComponent>() == false)
		{
			HY_CORE_ERROR("PrefabSerializer: can only save entities created with Scene::CreateEntity");
			return false;
		}

		// The root and everything under it, depth-first (parents before children).
		std::vector<Entity> subtree;
		entity.GetScene()->GetSubtree(entity, subtree);
		if (subtree.empty())
		{
			subtree.push_back(entity);
		}

		// The file's asset handle comes first (see SceneSerializer::Serialize).
		const AssetHandle handle = AssetManager::ImportAsset(filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		out << YAML::Key << "Prefab" << YAML::Value << entity.GetName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		for (size_t i = 0; i < subtree.size(); i++)
		{
			// The root's parent isn't in the file: it is placed by whoever instantiates it.
			EntitySerializer::Serialize(out, subtree[i], i != 0);
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::error_code ec;
		if (filepath.has_parent_path())
		{
			std::filesystem::create_directories(filepath.parent_path(), ec);
		}

		std::ofstream fout(filepath);
		if (fout.is_open() == false)
		{
			HY_CORE_ERROR("PrefabSerializer: could not open '{}' for writing", filepath.string());
			return false;
		}

		fout << out.c_str();
		HY_CORE_INFO("Saved prefab '{}' ({} entities) to {}", entity.GetName(), subtree.size(), filepath.string());
		return true;
	}

	Entity PrefabSerializer::Instantiate(Scene& scene, const std::filesystem::path& filepath, Entity parent)
	{
		Entity entity = Load(scene, filepath, parent);
		if (entity)
		{
			HY_CORE_INFO("Instantiated prefab '{}' from {}", entity.GetName(), filepath.string());
		}

		return entity;
	}

	Entity PrefabSerializer::Instantiate(Scene& scene, const Prefab& prefab, Entity parent)
	{
		const std::vector<YAML::Node>& nodes = prefab.GetEntityNodes();
		if (nodes.empty())
		{
			return {};
		}

		// Created in file order (depth-first), each with a new UUID. A saved parent is another entity of the
		// prefab, listed before it: the entity becomes that copy's last child, which keeps the subtree's order. A
		// parent the prefab doesn't contain (a hand-edited file) makes it a child of the root.
		std::vector<Entity> created;
		std::unordered_map<UUID, UUID> remap;
		std::unordered_map<UUID, Entity> bySavedId;
		created.reserve(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++)
		{
			const YAML::Node& node = nodes[i];
			Entity entity = EntitySerializer::Deserialize(node, scene, false);
			if (i > 0)
			{
				Entity savedParent = created.front();
				if (auto relationshipNode = node["RelationshipComponent"])
				{
					const auto found = bySavedId.find(UUID(relationshipNode["Parent"].as<uint64_t>(0)));
					if (found != bySavedId.end())
					{
						savedParent = found->second;
					}
				}
				scene.SetParent(entity, savedParent, false);
			}

			if (auto idNode = node["Entity"])
			{
				const UUID savedId(idNode.as<uint64_t>(0));
				bySavedId[savedId] = entity;
				remap[savedId] = entity.GetUUID();
			}
			created.push_back(entity);
		}

		// Any other reference between the prefab's entities follows the copies too.
		scene.RemapEntityReferences(created, remap);

		Entity root = created.front();
		if (parent)
		{
			// Its saved transform is relative to the new parent (it isn't kept in place in the world).
			scene.SetParent(root, parent, false);
		}
		return root;
	}

	Entity PrefabSerializer::Instantiate(Scene& scene, AssetHandle prefab, Entity parent)
	{
		const Ref<Prefab> loaded = AssetManager::GetAsset<Prefab>(prefab);
		if (loaded == nullptr)
		{
			return {};
		}
		return Instantiate(scene, *loaded, parent);
	}

	Entity PrefabSerializer::Load(Scene& scene, const std::filesystem::path& filepath, Entity parent)
	{
		// The file is read the way the AssetManager reads it (one format, one reader); not shared.
		const Ref<Prefab> prefab = Prefab::LoadUncached(filepath);
		if (prefab == nullptr)
		{
			return {};
		}
		return Instantiate(scene, *prefab, parent);
	}
}
