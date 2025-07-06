#include "hypch.h"
#include "Entity.h"

namespace Hydro
{
	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{
	}
}
