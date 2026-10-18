#pragma once
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/ScriptRegistry.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Hydro
{
	// Base class for native (C++) gameplay scripts. Derive from it and attach with
	// entity.AddComponent<NativeScriptComponent>().Bind<MyScript>();
	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		Entity GetEntity() const { return m_Entity; }

	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate([[maybe_unused]] float deltaTime) {}

	private:
		// Held by value: an Entity is just a handle + scene pointer, so it never dangles
		// when the registry moves its component storage around.
		Entity m_Entity;
		friend class Scene;
	};

	// Plain data stored in the registry: the entity's scripts, in the order they were bound.
	// Script instances live on the heap, so their address stays stable even when EnTT moves this
	// component in memory. An entity has one of these, holding as many scripts as it needs.
	struct NativeScriptComponent
	{
		struct Script
		{
			// The ScriptRegistry name. Saved with the scene. Empty when the type was bound in code
			// without being registered: it still runs, but isn't saved.
			std::string Name;
			// Null when Name isn't registered (e.g. a scene saved with a script that no longer exists):
			// the entry is kept so the scene saves it back unchanged, but it never runs.
			ScriptableEntity* (*Instantiate)() = nullptr;
			std::unique_ptr<ScriptableEntity> Instance;
		};

		std::vector<Script> Scripts;

		// Binds a script by type. The script is created on the scene's next runtime update
		// (OnCreate runs then). Register the type with ScriptRegistry for it to be saved.
		template<typename T>
		NativeScriptComponent& Bind()
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "Scripts must derive from ScriptableEntity");

			Script script;
			script.Name = ScriptRegistry::GetName(typeid(T));
			script.Instantiate = []() -> ScriptableEntity* { return new T(); };
			Scripts.push_back(std::move(script));
			return *this;
		}

		// Binds a registered script by name (used when loading scenes and by the editor). An unknown
		// name is kept but never runs.
		NativeScriptComponent& Bind(const std::string& name)
		{
			Script script;
			script.Name = name;
			script.Instantiate = ScriptRegistry::Find(name);
			Scripts.push_back(std::move(script));
			return *this;
		}

		bool Has(const std::string& name) const
		{
			for (const Script& script : Scripts)
			{
				if (script.Name == name)
					return true;
			}
			return false;
		}
	};
}
