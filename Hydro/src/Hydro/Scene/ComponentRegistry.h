#pragma once

#include "Hydro/Data/DataType.h"
#include "Hydro/Scene/Entity.h"

#include <entt.hpp>

#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	// A game (or engine) component declared with HY_COMPONENT(): plain data with reflected fields. Registered, it is
	// saved, copied, remapped and listed in Add Component (docs/SceneAndECS.md, "Game components").
	struct ComponentTypeInfo
	{
		// The saved key (the struct's name) and the inspector's label ("Waypoint" for WaypointComponent).
		std::string Name;
		std::string DisplayName;
		// Keys scenes may still have it under (HY_COMPONENT(FormerName("..."))): read, logged, saved as Name.
		std::vector<std::string> FormerNames;
		std::type_index CppType = typeid(void);
		// Its fields: a Struct DataValueType whose DataTypeInfo is in the DataTypeRegistry.
		DataValueType Type;

		bool (*Has)(const Entity& entity) = nullptr;
		void* (*Get)(Entity& entity) = nullptr;   // nullptr when the entity hasn't one
		void* (*Add)(Entity& entity) = nullptr;   // a default one (replacing one it had)
		void (*Remove)(Entity& entity) = nullptr;
		// Scene::Copy: every one in `source` onto the entity `entityMap` maps its owner to.
		void (*CopyAll)(entt::registry& destination, const entt::registry& source,
			const std::unordered_map<entt::entity, entt::entity>& entityMap) = nullptr;
		// Scene::DuplicateEntity: the entity's one (if any) onto `destination` in the same registry.
		void (*CopyOne)(entt::registry& registry, entt::entity source, entt::entity destination) = nullptr;
	};

	class ComponentRegistry
	{
	public:
		// Generated (RegisterGeneratedScriptReflection) for each HY_COMPONENT struct.
		template<typename T>
		static void Register(const std::string& name, const std::string& displayName, std::vector<std::string> formerNames = {});

		// In registration order (the generator sorts by header).
		static const std::vector<ComponentTypeInfo>& GetTypes();
		// By saved name or a former name; nullptr when none.
		static const ComponentTypeInfo* Find(const std::string& name);
		static const ComponentTypeInfo* Find(std::type_index type);

	private:
		static void Add(ComponentTypeInfo&& info);
	};

	template<typename T>
	void ComponentRegistry::Register(const std::string& name, const std::string& displayName, std::vector<std::string> formerNames)
	{
		static_assert(std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>,
			"An HY_COMPONENT must be default-constructible and copyable");
		ComponentTypeInfo info;
		info.Name = name;
		info.DisplayName = displayName;
		info.FormerNames = std::move(formerNames);
		info.CppType = typeid(T);
		info.Type = MakeDataValueType<T>();
		info.Has = [](const Entity& entity) -> bool
		{
			return entity.HasComponent<T>();
		};
		info.Get = [](Entity& entity) -> void*
		{
			return entity.TryGetComponent<T>();
		};
		info.Add = [](Entity& entity) -> void*
		{
			if (entity.HasComponent<T>())
			{
				entity.RemoveComponent<T>();
			}
			return &entity.AddComponent<T>();
		};
		info.Remove = [](Entity& entity)
		{
			if (entity.HasComponent<T>())
			{
				entity.RemoveComponent<T>();
			}
		};
		info.CopyAll = [](entt::registry& destination, const entt::registry& source,
			const std::unordered_map<entt::entity, entt::entity>& entityMap)
		{
			auto view = source.view<const T>();
			for (auto sourceEntity : view)
			{
				auto it = entityMap.find(sourceEntity);
				if (it != entityMap.end())
				{
					destination.emplace_or_replace<T>(it->second, view.template get<const T>(sourceEntity));
				}
			}
		};
		info.CopyOne = [](entt::registry& registry, entt::entity source, entt::entity destination)
		{
			if (registry.has<T>(source))
			{
				// Copy first: emplacing can reallocate the pool the source lives in.
				T copy = registry.get<T>(source);
				registry.emplace_or_replace<T>(destination, std::move(copy));
			}
		};
		Add(std::move(info));
	}
}
