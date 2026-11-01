#pragma once

#include"Scene.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Core/UUID.h"
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace Hydro
{
	class Entity;
	class ScriptableEntity;

	// True for a script type (a class deriving from ScriptableEntity, which declares ScriptTypeTag); false for an
	// ECS component. Entity's component functions look script types up in the entity's NativeScriptComponent.
	template<typename T, typename = void>
	struct IsScriptType : std::false_type
	{
	};

	template<typename T>
	struct IsScriptType<T, std::void_t<typename T::ScriptTypeTag>> : std::true_type
	{
	};

	// Entity's lookups of the scripts on an entity (Scripting plan SC3). Declared here and defined in
	// ScriptableEntity.h, which every script type includes, so Entity.h doesn't need the script headers.
	struct ScriptLookup
	{
		// The entity's running script that is a T (the script's own class or a class it derives from), or nullptr.
		// Two scripts deriving from the same T: the first, with a warning once.
		template<typename T>
		static T* Find(const Entity& entity);

		// Every running script of the entity that is a T, in binding order.
		template<typename T>
		static std::vector<T*> FindAll(const Entity& entity);

		// A running script that is a T, or a binding of exactly T without an instance yet (outside Play, or before
		// the script system creates it).
		template<typename T>
		static bool Has(const Entity& entity);
	};

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
		// GetComponent, that it has the component: check HasComponent, or use TryGetComponent, when it may not.
		//
		// GetComponent, TryGetComponent, HasComponent and GetComponents also take a script type (Scripting plan SC3):
		// `entity.GetComponent<DialogueScript>()` is the entity's running script of that class (or of a class deriving
		// from it). Scripts run only while playing, so outside Play the script lookups find nothing and HasComponent
		// means "bound". Scripts are bound with NativeScriptComponent::Bind, not AddComponent.
		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			static_assert(IsScriptType<T>::value == false, "Bind scripts with NativeScriptComponent::Bind<T>(), not AddComponent");
			HY_CORE_ASSERT(IsAlive(), "Entity::AddComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			HY_CORE_ASSERT(HasComponent<T>() == false, "Entity already has component {0}", typeid(T).name());
			T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		T& GetComponent()
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::GetComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			if constexpr (IsScriptType<T>::value)
			{
				T* script = ScriptLookup::Find<T>(*this);
				HY_CORE_ASSERT(script != nullptr, "Entity has no running script {0}", typeid(T).name());
				return *script;
			}
			else
			{
				HY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component {0}", typeid(T).name());
				return m_Scene->m_Registry.get<T>(m_EntityHandle);
			}
		}

		template<typename T>
		const T& GetComponent() const
		{
			HY_CORE_ASSERT(IsAlive(), "Entity::GetComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			if constexpr (IsScriptType<T>::value)
			{
				const T* script = ScriptLookup::Find<T>(*this);
				HY_CORE_ASSERT(script != nullptr, "Entity has no running script {0}", typeid(T).name());
				return *script;
			}
			else
			{
				HY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component {0}", typeid(T).name());
				return std::as_const(m_Scene->m_Registry).get<T>(m_EntityHandle);
			}
		}

		// The component (or running script), or nullptr when the entity has none or doesn't exist.
		template<typename T>
		T* TryGetComponent()
		{
			if (IsAlive() == false)
			{
				return nullptr;
			}
			if constexpr (IsScriptType<T>::value)
			{
				return ScriptLookup::Find<T>(*this);
			}
			else
			{
				return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
			}
		}

		template<typename T>
		const T* TryGetComponent() const
		{
			if (IsAlive() == false)
			{
				return nullptr;
			}
			if constexpr (IsScriptType<T>::value)
			{
				return ScriptLookup::Find<T>(*this);
			}
			else
			{
				return std::as_const(m_Scene->m_Registry).try_get<T>(m_EntityHandle);
			}
		}

		// Every running script of the entity that is a T (its own class or one it derives from), in binding order.
		// Script types only: an entity has at most one ECS component of a type.
		template<typename T>
		std::vector<T*> GetComponents() const
		{
			static_assert(IsScriptType<T>::value, "GetComponents takes a script type; use TryGetComponent for an ECS component");
			if (IsAlive() == false)
			{
				return {};
			}
			return ScriptLookup::FindAll<T>(*this);
		}

		template<typename T>
		void RemoveComponent()
		{
			static_assert(IsScriptType<T>::value == false, "Remove scripts through NativeScriptComponent (Scene::RemoveScript), not RemoveComponent");
			HY_CORE_ASSERT(IsAlive(), "Entity::RemoveComponent<{0}> on an entity that doesn't exist", typeid(T).name());
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		// False for an empty or destroyed entity instead of failing.
		template<typename T>
		bool HasComponent() const
		{
			if constexpr (IsScriptType<T>::value)
			{
				return IsAlive() && ScriptLookup::Has<T>(*this);
			}
			else
			{
				return IsAlive() && m_Scene->m_Registry.has<T>(m_EntityHandle);
			}
		}

		Scene* GetScene() const
		{
			return m_Scene;
		}

		// The descendant at `path`: child names (tags) separated by '/', each a direct child of the one before
		// ("Panel/Buttons/Ok"); the first child of that name at each step. An empty Entity when a step isn't found
		// or this entity doesn't exist. For the inside of a view or prefab, whose names its author controls
		// (Scripting plan SC4); a dependency on another part of the scene is an EntityRef property instead.
		Entity FindChild(const std::string& path) const;

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
