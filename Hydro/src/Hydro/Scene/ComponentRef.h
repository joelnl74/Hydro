#pragma once

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/EntityRef.h"

namespace Hydro
{
	// A reference to a component or script on another entity: an EntityRef whose target must have a T. Get() looks it
	// up each call: null when empty, gone, or without a T (a script only while playing).
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
