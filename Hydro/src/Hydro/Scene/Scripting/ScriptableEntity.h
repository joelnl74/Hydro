#pragma once

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scripting/ScriptProperty.h"
#include "Hydro/Scene/Scripting/ScriptRegistry.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Hydro
{
	class Scene;
	struct SystemContext;

	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		Entity GetEntity() const
		{
			return m_Entity;
		}

		// Where the entity is in the world, whatever its parents. TransformComponent::Translation is relative to the
		// parent, so a script that places things in the world, or compares positions, uses these; for another entity,
		// Scene::GetWorldPosition / SetWorldPosition.
		glm::vec3 GetWorldPosition()
		{
			return m_Entity.GetScene()->GetWorldPosition(m_Entity);
		}

		// Moves the entity so its world position becomes `position`; only its local Translation changes (see
		// Scene::SetWorldPosition). False, moving nothing, without a TransformComponent.
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

	private:
		Entity m_Entity;

		friend class Scene;
		friend void UpdateScripts(Scene&, const SystemContext&);
	};

	struct NativeScriptComponent
	{
		struct Script
		{
			std::string Name;

			ScriptableEntity* (*Instantiate)() = nullptr;

			std::unique_ptr<ScriptableEntity> Instance;

			// Values belonging to this particular component instance.
			//
			// Example:
			//
			// EnemyController
			//     MoveSpeed = 8
			//     Health = 250
			//
			// These exist even when Instance == nullptr.
			ScriptPropertyMap Properties;

			bool PendingRemove = false;
		};

		std::vector<Script> Scripts;

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

			// If this is registered, populate its editable
			// properties with the C++ defaults.
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

			script.Name = name;

			script.Instantiate =
				ScriptRegistry::Find(name);

			// Unknown scripts remain valid entries,
			// but registered ones get their defaults.
			ScriptRegistry::InitializeProperties(
				name,
				script.Properties);

			Scripts.push_back(
				std::move(script));

			return *this;
		}

		bool Has(const std::string& name) const
		{
			for (const Script& script : Scripts)
			{
				if (script.Name == name)
				{
					return true;
				}
			}

			return false;
		}
	};
}
