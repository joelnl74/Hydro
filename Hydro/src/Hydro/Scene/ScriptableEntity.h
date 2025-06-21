#pragma once
#include "Entity.h"

namespace Hydro
{
	class SciptableEntity
	{
	public:
		template<typename T>
		T& GetComponent()
		{
			return m_entity->GetComponent<T>();
		}

		virtual void OnCreate() {}
		virtual void OnUpdate(float deltaTime) {}
		virtual void OnDestroy() {}


	private:
		Entity* m_entity;
		friend class Scene;
	};
}
