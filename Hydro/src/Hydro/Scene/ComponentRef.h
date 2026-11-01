#pragma once

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/EntityRef.h"

namespace Hydro
{
	// A reference to a component or a script on another entity (Scripting plan SC4): an EntityRef whose target must
	// have a T. In the editor the picker lists only entities that have one (a script bound there counts), and a
	// target without one shows in the warning colour. Get() finds the entity and its T each call, so it is never
	// stale: nullptr when the reference is empty, the entity is gone, or it has no T (a script only while playing).
	//
	//     HY_PROPERTY(Required) ComponentRef<DialogueScript> Dialogue;
	//     ...
	//     if (DialogueScript* dialogue = Dialogue.Get()) { dialogue->Open(lines); }
	template<typename T>
	class ComponentRef
	{
	public:
		using Target = T;

		ComponentRef() = default;
		explicit ComponentRef(const EntityRef& entity)
			: m_Entity(entity)
		{
		}

		// The target's T, or nullptr.
		T* Get() const
		{
			Entity entity = m_Entity.Get();
			if ((bool)entity == false)
			{
				return nullptr;
			}
			return entity.TryGetComponent<T>();
		}

		T* operator->() const
		{
			T* target = Get();
			HY_CORE_ASSERT(target != nullptr, "ComponentRef<{0}>: the reference is empty or its entity has no {0}", typeid(T).name());
			return target;
		}

		// The target entity (whether or not it has a T now), or an empty Entity.
		Entity GetEntity() const { return m_Entity.Get(); }

		const EntityRef& GetEntityRef() const { return m_Entity; }
		EntityRef& GetEntityRef() { return m_Entity; }

		bool IsSet() const { return m_Entity.IsSet(); }

	private:
		EntityRef m_Entity;
	};

	template<typename T>
	struct IsComponentRef : std::false_type
	{
	};

	template<typename T>
	struct IsComponentRef<ComponentRef<T>> : std::true_type
	{
	};
}
