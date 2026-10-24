#pragma once

#include"Scene.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Core/UUID.h"
#include <typeinfo>
#include <utility>

namespace Hydro
{
	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity handle, Scene* scene);
		Entity(const Entity& other) = default;

		// True when the handle refers to a live entity of a scene that isn't about to remove it: not
		// empty, not destroyed, and not queued for destruction by Scene::DestroyEntity during a stage
		// (see Scene::IsPendingDestroy). Check it before using an Entity you kept from an earlier
		// frame or got from another system. An entity pending destruction can still be read until its
		// stage ends, so the component functions below only require it to exist.
		bool IsValid() const
		{
			return IsAlive() && m_Scene->IsPendingDestroy(*this) == false;
		}

		// Debug builds assert that the entity exists (AddComponent, RemoveComponent) and, for
		// GetComponent, that it has the component: check HasComponent when it may not.
		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::AddComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			HY_CORE_ASSERT(HasComponent<T>() == false, "Entity already has component {0}", typeid(T).name());
			T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		T& GetComponent()
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::GetComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			HY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component {0}", typeid(T).name());
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		const T& GetComponent() const
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::GetComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			HY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component {0}", typeid(T).name());
			return std::as_const(m_Scene->m_Registry).get<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::RemoveComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		// False for an empty or destroyed entity instead of failing.
		template<typename T>
		bool HasComponent() const
		{
			return IsAlive() && m_Scene->m_Registry.has<T>(m_EntityHandle);
		}

		Scene* GetScene()
		{
			return m_Scene;
		}

		UUID GetUUID() const { return GetComponent<IDComponent>().Id; }
		const std::string& GetName() const { return GetComponent<TagComponent>().Tag; }

		operator bool() const { return m_EntityHandle != entt::null; }
		operator entt::entity() const { return m_EntityHandle; }
		operator uint32_t() const { return (uint32_t)m_EntityHandle; }

		bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return (*this == other) == false; }

	private:
		// The scene's registry still holds this handle (it ignores pending destruction).
		bool IsAlive() const
		{
			return m_Scene != nullptr && m_EntityHandle != entt::null && m_Scene->m_Registry.valid(m_EntityHandle);
		}

		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;
	};
}
