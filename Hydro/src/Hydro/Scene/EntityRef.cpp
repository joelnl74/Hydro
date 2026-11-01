#include "hypch.h"
#include "EntityRef.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

namespace Hydro
{
	EntityRef::EntityRef(const Entity& entity)
	{
		if ((bool)entity && entity.HasComponent<IDComponent>())
		{
			m_Id = entity.GetUUID();
			m_Scene = entity.GetScene();
		}
	}

	Entity EntityRef::Get() const
	{
		if (m_Scene == nullptr || IsSet() == false)
		{
			return {};
		}
		return m_Scene->GetEntityByUUID(m_Id);
	}
}
