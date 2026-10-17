#pragma once
#include "Hydro/Scene/Entity.h"

#include <memory>
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
		virtual void OnUpdate(float deltaTime) {}

	private:
		// Held by value: an Entity is just a handle + scene pointer, so it never dangles
		// when the registry moves its component storage around.
		Entity m_Entity;
		friend class Scene;
	};

	// Plain data stored in the registry. Script instances live on the heap, so their address
	// stays stable even when EnTT moves this component in memory.
	struct NativeScriptComponent
	{
		struct Script
		{
			std::unique_ptr<ScriptableEntity> Instance;
			ScriptableEntity* (*Instantiate)() = nullptr;
		};

		std::vector<Script> Scripts;

		// The script is created on the scene's next update (OnCreate runs then).
		template<typename T>
		NativeScriptComponent& Bind()
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "Scripts must derive from ScriptableEntity");

			Script script;
			script.Instantiate = []() -> ScriptableEntity* { return new T(); };
			Scripts.push_back(std::move(script));
			return *this;
		}
	};
}
