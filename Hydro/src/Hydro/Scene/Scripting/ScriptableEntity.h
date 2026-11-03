#pragma once

#include "Hydro/Core/Log.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scripting/ScriptProperty.h"
#include "Hydro/Scene/Scripting/ScriptRegistry.h"
#include "Hydro/Scene/UI/UIEvent.h"

#include <glm/glm.hpp>

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Hydro
{
	class Scene;
	class Gizmos;
	struct SystemContext;

	class ScriptableEntity
	{
	public:
		// Marks script types for Entity's component functions (IsScriptType).
		using ScriptTypeTag = ScriptableEntity;

		virtual ~ScriptableEntity() = default;

		// This entity's components and scripts (a script type finds its running script of that class). For another
		// entity, call `other.GetComponent<T>()`.
		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		template<typename T>
		T* TryGetComponent()
		{
			return m_Entity.TryGetComponent<T>();
		}

		template<typename T>
		bool HasComponent() const
		{
			return m_Entity.HasComponent<T>();
		}

		Entity GetEntity() const
		{
			return m_Entity;
		}

		// The world position, whatever the parents (TransformComponent::Translation is relative to the parent). For
		// another entity: Scene::GetWorldPosition / SetWorldPosition.
		glm::vec3 GetWorldPosition()
		{
			return m_Entity.GetScene()->GetWorldPosition(m_Entity);
		}

		// Only the local Translation changes. False, moving nothing, without a TransformComponent.
		bool SetWorldPosition(const glm::vec3& position)
		{
			return m_Entity.GetScene()->SetWorldPosition(m_Entity, position);
		}

	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate([[maybe_unused]] float deltaTime) {}

		virtual void OnCollisionEnter([[maybe_unused]] Entity other) {}
		virtual void OnCollisionExit([[maybe_unused]] Entity other) {}
		virtual void OnTriggerEnter([[maybe_unused]] Entity other) {}
		virtual void OnTriggerExit([[maybe_unused]] Entity other) {}

		// A UI event (Submit, Cancel, FocusEnter, FocusExit) for this entity or a UI element below it. Return true to stop
		// it here, false to pass it to the parent's scripts. Runs at stage 0, before OnUpdate.
		virtual bool OnUIEvent([[maybe_unused]] const UIEvent& event) { return false; }

		// Editor-view lines and text, also outside Play (Gizmos.h). Outside Play it runs on a fresh instance with saved
		// properties and no OnCreate, so draw from properties and the scene. The generator registers overriders.
		virtual void OnDrawGizmos([[maybe_unused]] Gizmos& gizmos) {}

	private:
		Entity m_Entity;

		friend class Scene;
		friend void UpdateScripts(Scene&, const SystemContext&);
		friend void CollectScriptGizmos(Scene&, const Entity&, Gizmos&);
	};

	struct NativeScriptComponent
	{
		struct Script
		{
			std::string Name;

			ScriptableEntity* (*Instantiate)() = nullptr;

			std::unique_ptr<ScriptableEntity> Instance;

			// This binding's property values (MoveSpeed = 8, Health = 250); they exist even when Instance is null.
			ScriptPropertyMap Properties;

			// Saved values no property reads (unregistered script, renamed or removed property), as YAML text by key. Kept
			// so saving doesn't lose them (#143); ScriptSerializer fills them.
			std::map<std::string, std::string> KeptProperties;

			bool PendingRemove = false;
		};

		std::vector<Script> Scripts;

		// Binds a script. An entity holds one script per class (different classes sharing a base are fine); binding a
		// class it already has is refused with a warning.
		template<typename T>
		NativeScriptComponent& Bind()
		{
			static_assert(
				std::is_base_of_v<ScriptableEntity, T>,
				"Scripts must derive from ScriptableEntity");

			Script script;

			script.Name = ScriptRegistry::GetName(typeid(T));

			script.Instantiate =
				[]() -> ScriptableEntity*
			{
				return new T();
			};

			for (const Script& existing : Scripts)
			{
				const bool sameName = script.Name.empty() == false && existing.Name == script.Name;
				const bool sameFactory = script.Name.empty() && existing.Instantiate == script.Instantiate;
				if (existing.PendingRemove == false && (sameName || sameFactory))
				{
					HY_CORE_WARN("NativeScriptComponent::Bind: the entity already has a {}; an entity holds one script of a class",
						script.Name.empty() ? typeid(T).name() : script.Name);
					return *this;
				}
			}

			// A registered script starts with the C++ defaults as its editable properties.
			if (script.Name.empty() == false)
			{
				ScriptRegistry::InitializeProperties(
					script.Name,
					script.Properties);
			}

			Scripts.push_back(
				std::move(script));

			return *this;
		}

		NativeScriptComponent& Bind(const std::string& name)
		{
			Script script;

			// A former name binds the script it now belongs to, under its current name.
			const ScriptTypeInfo* typeInfo =
				ScriptRegistry::FindType(name);

			script.Name = typeInfo != nullptr
				? typeInfo->Name
				: name;

			script.Instantiate = typeInfo != nullptr
				? typeInfo->Factory
				: nullptr;

			if (Has(script.Name))
			{
				HY_CORE_WARN("NativeScriptComponent::Bind: the entity already has a {}; an entity holds one script of a class",
					script.Name);
				return *this;
			}

			// Unknown scripts stay valid entries; registered ones get their defaults.
			ScriptRegistry::InitializeProperties(
				script.Name,
				script.Properties);

			Scripts.push_back(
				std::move(script));

			return *this;
		}

		// A binding of the script registered (or saved) under this current name, not being removed.
		bool Has(const std::string& name) const
		{
			for (const Script& script : Scripts)
			{
				if (script.PendingRemove == false && script.Name == name)
				{
					return true;
				}
			}

			return false;
		}
	};

	// ---- ScriptLookup (declared in Entity.h) -------------------------------------------------------------------

	template<typename T>
	std::vector<T*> ScriptLookup::FindAll(const Entity& entity)
	{
		static_assert(std::is_base_of_v<ScriptableEntity, T>, "Script lookups take a class deriving from ScriptableEntity");

		std::vector<T*> found;
		if (entity.HasComponent<NativeScriptComponent>() == false)
		{
			return found;
		}
		for (const NativeScriptComponent::Script& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.PendingRemove)
			{
				continue;
			}
			if (T* instance = dynamic_cast<T*>(script.Instance.get()))
			{
				found.push_back(instance);
			}
		}
		return found;
	}

	template<typename T>
	T* ScriptLookup::Find(const Entity& entity)
	{
		static_assert(std::is_base_of_v<ScriptableEntity, T>, "Script lookups take a class deriving from ScriptableEntity");

		if (entity.HasComponent<NativeScriptComponent>() == false)
		{
			return nullptr;
		}
		T* found = nullptr;
		for (const NativeScriptComponent::Script& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.PendingRemove)
			{
				continue;
			}
			T* instance = dynamic_cast<T*>(script.Instance.get());
			if (instance == nullptr)
			{
				continue;
			}
			if (found == nullptr)
			{
				found = instance;
				continue;
			}
			// Two scripts are a T: only a base class can match twice (one script per class).
			static bool s_Warned = false;
			if (s_Warned == false)
			{
				s_Warned = true;
				HY_CORE_WARN("Entity '{}' has several scripts deriving from {}; GetComponent returns the first (use GetComponents for all)",
					entity.GetName(), typeid(T).name());
			}
			break;
		}
		return found;
	}

	template<typename T>
	bool ScriptLookup::Has(const Entity& entity)
	{
		static_assert(std::is_base_of_v<ScriptableEntity, T>, "Script lookups take a class deriving from ScriptableEntity");

		if (entity.HasComponent<NativeScriptComponent>() == false)
		{
			return false;
		}
		const std::string& name = ScriptRegistry::GetName(typeid(T));
		for (const NativeScriptComponent::Script& script : entity.GetComponent<NativeScriptComponent>().Scripts)
		{
			if (script.PendingRemove)
			{
				continue;
			}
			if (script.Instance != nullptr)
			{
				if (dynamic_cast<T*>(script.Instance.get()) != nullptr)
				{
					return true;
				}
			}
			else if (name.empty() == false && script.Name == name)
			{
				return true;
			}
		}
		return false;
	}
}
