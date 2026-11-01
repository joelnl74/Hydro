#include "hypch.h"
#include "Entity.h"

namespace Hydro
{
	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene)
	{
	}

	Entity Entity::FindChild(const std::string& path) const
	{
		if (IsAlive() == false)
		{
			return {};
		}

		Entity current = *this;
		std::vector<Entity> children;
		size_t start = 0;
		while (start <= path.size())
		{
			size_t end = path.find('/', start);
			if (end == std::string::npos)
			{
				end = path.size();
			}
			const std::string name = path.substr(start, end - start);
			start = end + 1;
			if (name.empty())
			{
				// "A//B" or a leading / trailing '/': skip the empty step.
				if (end == path.size())
				{
					break;
				}
				continue;
			}

			m_Scene->GetChildren(current, children);
			Entity next;
			for (const Entity& child : children)
			{
				if (child.HasComponent<TagComponent>() && child.GetComponent<TagComponent>().Tag == name)
				{
					next = child;
					break;
				}
			}
			if ((bool)next == false)
			{
				return {};
			}
			current = next;
			if (end == path.size())
			{
				break;
			}
		}
		return current == *this ? Entity{} : current;
	}
}
