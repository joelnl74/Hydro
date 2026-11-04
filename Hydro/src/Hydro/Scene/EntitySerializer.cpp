#include "hypch.h"
#include "EntitySerializer.h"
#include "ComponentRegistry.h"
#include "Hydro/Data/DataSerializer.h"
#include "Scripting/ScriptableEntity.h"
#include "Scripting/ScriptSerializer.h"

#include <string>

namespace Hydro
{
	void EntitySerializer::Serialize(YAML::Emitter& out, Entity entity, bool includeParent)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << (uint64_t)entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>();

			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Tag" << YAML::Value << tag.Tag;
			out << YAML::EndMap;
		}

		// Roots have none (and nothing is written).
		if (includeParent && entity.HasComponent<RelationshipComponent>()
			&& (uint64_t)entity.GetComponent<RelationshipComponent>().Parent != 0)
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Parent" << YAML::Value << (uint64_t)entity.GetComponent<RelationshipComponent>().Parent;
			out << YAML::EndMap;
		}

		// The engine's components, in registration order (ComponentSerializers).
		for (const EngineComponentInfo& type : ComponentRegistry::GetEngineTypes())
		{
			if (type.Save != nullptr && type.Has(entity))
			{
				out << YAML::Key << type.Name;
				out << YAML::BeginMap;
				type.Save(out, entity);
				out << YAML::EndMap;
			}
		}

		// The game's own components (HY_COMPONENT): their fields as a data file writes a struct.
		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			if (const void* data = type.Get(entity))
			{
				out << YAML::Key << type.Name << YAML::Value;
				DataSerializer::WriteValue(out, type.Type, data);
			}
		}

		if (entity.HasComponent<NativeScriptComponent>())
		{
			out << YAML::Key << "NativeScriptComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "Scripts" << YAML::Value;
			ScriptSerializer::Write(out, entity.GetComponent<NativeScriptComponent>(), entity.GetName());
			out << YAML::EndMap;
		}

		out << YAML::EndMap; // Entity
	}

	Entity EntitySerializer::Deserialize(const YAML::Node& entityNode, Scene& scene, bool keepUUID)
	{
		std::string name;
		if (auto tagNode = entityNode["TagComponent"])
		{
			name = tagNode["Tag"].as<std::string>("");
		}

		UUID uuid; // fresh random id by default
		if (keepUUID && entityNode["Entity"])
		{
			uuid = UUID(entityNode["Entity"].as<uint64_t>());
		}

		// CreateEntityWithUUID adds the Tag + ID components.
		Entity entity = scene.CreateEntityWithUUID(uuid, name);

		// Only when loading a scene: the parent is in the same file, and the loader orders the list afterwards
		// (Scene::RepairHierarchy). Prefab instances get new UUIDs and are linked by PrefabSerializer::Instantiate.
		if (keepUUID)
		{
			if (auto relationshipNode = entityNode["RelationshipComponent"])
			{
				const uint64_t parent = relationshipNode["Parent"].as<uint64_t>(0);
				if (parent != 0)
				{
					entity.AddComponent<RelationshipComponent>(UUID(parent));
				}
			}
		}

		// The engine's components: missing keys keep the defaults; values that would break a system are pulled into range.
		for (const EngineComponentInfo& type : ComponentRegistry::GetEngineTypes())
		{
			if (type.Load == nullptr)
			{
				continue;
			}
			if (const YAML::Node componentNode = entityNode[type.Name])
			{
				type.Load(componentNode, entity);
			}
		}

		// The game's own components (HY_COMPONENT): under their name, or a former one (logged; saving writes the name).
		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			std::string key = type.Name;
			YAML::Node componentNode = entityNode[key];
			for (size_t i = 0; componentNode.IsDefined() == false && i < type.FormerNames.size(); i++)
			{
				key = type.FormerNames[i];
				componentNode = entityNode[key];
			}
			if (componentNode.IsDefined() == false)
			{
				continue;
			}
			if (key != type.Name)
			{
				HY_CORE_INFO("Entity '{}': {} is saved under its former name '{}'; saving writes '{}'", name, type.Name, key, type.Name);
			}
			void* data = type.Add(entity);
			if (componentNode.IsNull() == false)
			{
				DataReport report;
				DataSerializer::ReadValue(componentNode, type.Type, data, report, type.Name);
				report.Log("Entity '" + name + "'");
			}
		}

		if (auto scriptNode = entityNode["NativeScriptComponent"])
		{
			auto& scripts = entity.AddComponent<NativeScriptComponent>();
			ScriptSerializer::Read(scriptNode["Scripts"], scripts, entity.GetName());
		}

		return entity;
	}
}
