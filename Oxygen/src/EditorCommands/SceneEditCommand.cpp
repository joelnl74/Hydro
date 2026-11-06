#include "SceneEditCommand.h"

#include <Hydro/Scene/EntitySerializer.h>

#include <algorithm>

namespace Hydro
{
	static Entity FindLive(Scene& scene, UUID id)
	{
		if ((uint64_t)id == 0)
		{
			return {};
		}
		Entity entity = scene.GetEntityByUUID(id);
		return (bool)entity && entity.IsValid() ? entity : Entity{};
	}

	// The parent's children, or the roots, in list order.
	static void GetSiblings(Scene& scene, Entity parent, std::vector<Entity>& siblings)
	{
		siblings.clear();
		if (parent)
		{
			scene.GetChildren(parent, siblings);
			return;
		}
		for (entt::entity handle : scene.GetEntityOrder())
		{
			Entity entity{ handle, &scene };
			if ((bool)scene.GetParent(entity) == false)
			{
				siblings.push_back(entity);
			}
		}
	}

	std::string EntityState::SerializeComponents(Entity entity)
	{
		YAML::Emitter out;
		EntitySerializer::Serialize(out, entity, false);
		return out.c_str();
	}

	EntityState EntityState::Capture(Scene& scene, UUID id)
	{
		EntityState state;
		state.Id = id;
		Entity entity = FindLive(scene, id);
		if ((bool)entity == false)
		{
			return state;
		}
		state.Exists = true;
		state.Components = SerializeComponents(entity);
		Entity parent = scene.GetParent(entity);
		state.Parent = parent ? parent.GetUUID() : UUID(0);
		state.Depth = scene.GetDepth(entity);

		std::vector<Entity> siblings;
		GetSiblings(scene, parent, siblings);
		for (uint32_t i = 0; i < (uint32_t)siblings.size(); i++)
		{
			if (siblings[i] == entity)
			{
				state.SiblingIndex = i;
				break;
			}
		}
		return state;
	}

	SceneEditCommand::SceneEditCommand(const Ref<Scene>& scene, std::string name, std::vector<EntityState> before,
		std::vector<EntityState> after, UUID selectionBefore, UUID selectionAfter)
		: m_Scene(scene), m_Name(std::move(name)), m_Before(std::move(before)), m_After(std::move(after)),
		m_SelectionBefore(selectionBefore), m_SelectionAfter(selectionAfter)
	{
	}

	size_t SceneEditCommand::GetMemorySize() const
	{
		size_t size = sizeof(*this) + m_Name.size();
		for (const std::vector<EntityState>* states : { &m_Before, &m_After })
		{
			for (const EntityState& state : *states)
			{
				size += sizeof(EntityState) + state.Components.size();
			}
		}
		return size;
	}

	void SceneEditCommand::Apply(Scene& scene, const std::vector<EntityState>& states)
	{
		// 1. What should be absent goes (with what is under it: a deleted subtree was captured whole).
		for (const EntityState& state : states)
		{
			if (state.Exists == false)
			{
				if (Entity entity = FindLive(scene, state.Id))
				{
					scene.DestroyEntity(entity);
				}
			}
		}

		// 2. Missing entities come back with their UUIDs (as roots for now); the others get their components back.
		std::vector<const EntityState*> present;
		for (const EntityState& state : states)
		{
			if (state.Exists == false)
			{
				continue;
			}
			present.push_back(&state);
			const YAML::Node node = YAML::Load(state.Components);
			Entity entity = FindLive(scene, state.Id);
			if ((bool)entity == false)
			{
				EntitySerializer::Deserialize(node, scene, true);
			}
			else if (EntityState::SerializeComponents(entity) != state.Components)
			{
				EntitySerializer::ReplaceComponents(node, entity);
			}
		}

		// 3. Parents and places: parents before children, siblings from the first, so every index refers to a list
		// whose earlier places are already right.
		std::stable_sort(present.begin(), present.end(), [](const EntityState* a, const EntityState* b)
		{
			return a->Depth != b->Depth ? a->Depth < b->Depth : a->SiblingIndex < b->SiblingIndex;
		});
		for (const EntityState* state : present)
		{
			if (Entity entity = FindLive(scene, state->Id))
			{
				Place(scene, entity, *state);
			}
		}
	}

	void SceneEditCommand::Place(Scene& scene, Entity entity, const EntityState& state)
	{
		Entity parent = FindLive(scene, state.Parent);
		if (scene.GetParent(entity) != parent)
		{
			// The saved components already hold the local transform under that parent.
			scene.SetParent(entity, parent, false);
		}

		std::vector<Entity> siblings;
		GetSiblings(scene, parent, siblings);
		siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
		if (state.SiblingIndex < siblings.size())
		{
			scene.MoveEntity(entity, siblings[state.SiblingIndex], false);
		}
		else if (siblings.empty() == false)
		{
			scene.MoveEntity(entity, siblings.back(), true);
		}
	}

	SceneEditScope::SceneEditScope(const Ref<Scene>& scene, std::string name, UUID selectionBefore)
		: m_Scene(scene), m_Name(std::move(name)), m_SelectionBefore(selectionBefore)
	{
		if (m_Scene == nullptr)
		{
			return;
		}
		for (entt::entity handle : m_Scene->GetEntityOrder())
		{
			m_Existing.insert((uint64_t)Entity(handle, m_Scene.get()).GetUUID());
		}
	}

	void SceneEditScope::Add(UUID id, bool existedBefore)
	{
		if (m_Ids.insert((uint64_t)id).second == false)
		{
			return;
		}
		EntityState before;
		before.Id = id;
		if (existedBefore)
		{
			before = EntityState::Capture(*m_Scene, id);
		}
		m_Before.push_back(std::move(before));
	}

	void SceneEditScope::Capture(Entity entity, bool subtree)
	{
		if (m_Scene == nullptr || (bool)entity == false || entity.IsValid() == false)
		{
			return;
		}
		if (subtree == false)
		{
			Add(entity.GetUUID(), true);
			return;
		}
		std::vector<Entity> entities;
		m_Scene->GetSubtree(entity, entities);
		for (Entity member : entities)
		{
			Add(member.GetUUID(), true);
		}
	}

	Scope<SceneEditCommand> SceneEditScope::Finish(UUID selectionAfter)
	{
		if (m_Scene == nullptr)
		{
			return nullptr;
		}
		// What the edit created, in list order (parents before their children).
		for (entt::entity handle : m_Scene->GetEntityOrder())
		{
			const UUID id = Entity(handle, m_Scene.get()).GetUUID();
			if (m_Existing.count((uint64_t)id) == 0)
			{
				Add(id, false);
			}
		}
		std::vector<EntityState> after;
		after.reserve(m_Before.size());
		for (const EntityState& state : m_Before)
		{
			after.push_back(EntityState::Capture(*m_Scene, state.Id));
		}
		Scope<SceneEditCommand> command = CreateScope<SceneEditCommand>(m_Scene, m_Name, m_Before, std::move(after),
			m_SelectionBefore, selectionAfter);
		return command->IsEmpty() ? nullptr : std::move(command);
	}
}
