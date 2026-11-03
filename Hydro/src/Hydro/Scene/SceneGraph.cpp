#include <hypch.h>
#include "SceneGraph.h"
#include "Entity.h"
#include "Scene.h"

#include <algorithm>
#include <unordered_set>

namespace Hydro
{
	SceneGraph::SceneGraph(Scene& scene)
		: m_Scene(scene)
	{
	}

	void SceneGraph::Add(entt::entity handle)
	{
		// A root at the end of the list: the rule still holds.
		m_List.push_back(handle);
		m_Dirty = true;
	}

	void SceneGraph::Remove(entt::entity handle)
	{
		m_List.erase(std::remove(m_List.begin(), m_List.end(), handle), m_List.end());
		m_Dirty = true;
	}


	const std::vector<entt::entity>& SceneGraph::GetOrder()
	{
		// Drop entities that were destroyed straight through the registry.
		const size_t listed = m_List.size();
		m_List.erase(std::remove_if(m_List.begin(), m_List.end(), [&](entt::entity handle)
		{
			return m_Scene.m_Registry.valid(handle) == false || m_Scene.m_Registry.has<IDComponent>(handle) == false;
		}), m_List.end());
		if (m_List.size() != listed)
		{
			m_Dirty = true;
		}

		// Pick up entities that got an IDComponent without going through CreateEntity.
		if (m_Scene.m_Registry.size<IDComponent>() != m_List.size())
		{
			const std::unordered_set<entt::entity> known(m_List.begin(), m_List.end());
			std::vector<entt::entity> missing;
			for (entt::entity handle : m_Scene.m_Registry.view<IDComponent>())
			{
				if (known.find(handle) == known.end())
				{
					missing.push_back(handle);
				}
			}

			// The view lists newest first; the list is oldest first.
			m_List.insert(m_List.end(), missing.rbegin(), missing.rend());
			m_Dirty = true;
		}

		return m_List;
	}

	void SceneGraph::MoveEntity(Entity entity, Entity target, bool placeAfter)
	{
		if ((bool)entity == false || (bool)target == false || entity == target)
		{
			return;
		}

		const int32_t entityIndex = GetIndex((entt::entity)entity);
		const int32_t targetIndex = GetIndex((entt::entity)target);
		if (entityIndex < 0 || targetIndex < 0)
		{
			return;
		}
		if (m_Layout.IsAncestor((size_t)entityIndex, (size_t)targetIndex))
		{
			HY_CORE_WARN("Scene '{}': can't move '{}' next to '{}', which is inside it", m_Scene.GetName(), entity.GetName(),
				target.GetName());
			return;
		}

		// It becomes target's sibling, staying where it is in the world.
		UUID parent = 0;
		if (m_Scene.m_Registry.has<RelationshipComponent>(target))
		{
			parent = m_Scene.m_Registry.get<RelationshipComponent>(target).Parent;
		}
		UUID currentParent = 0;
		if (m_Scene.m_Registry.has<RelationshipComponent>(entity))
		{
			currentParent = m_Scene.m_Registry.get<RelationshipComponent>(entity).Parent;
		}
		const bool keepWorld = parent != currentParent && m_Scene.m_Registry.has<TransformComponent>(entity);
		const glm::mat4 world = keepWorld ? m_Scene.GetWorldTransform(entity) : glm::mat4(1.0f);

		const size_t insertAt = m_Layout.GetSiblingInsertIndex((size_t)targetIndex, placeAfter);
		SetParentUUID(entity, parent);
		MoveSubtree((size_t)entityIndex, insertAt);
		if (keepWorld)
		{
			m_Scene.SetWorldTransform(entity, world);
		}
		CheckAfterEdit();
	}

	std::vector<SceneHierarchy::Node> SceneGraph::MakeNodes()
	{
		std::vector<SceneHierarchy::Node> nodes;
		nodes.reserve(m_List.size());
		for (entt::entity handle : m_List)
		{
			SceneHierarchy::Node node;
			node.Id = m_Scene.m_Registry.get<IDComponent>(handle).Id;
			if (m_Scene.m_Registry.has<RelationshipComponent>(handle))
			{
				node.Parent = m_Scene.m_Registry.get<RelationshipComponent>(handle).Parent;
			}
			nodes.push_back(node);
		}
		return nodes;
	}

	void SceneGraph::Ensure()
	{
		// Drops destroyed entities and picks up unlisted ones (marks the hierarchy dirty if it did).
		GetOrder();
		if (m_Dirty == false)
		{
			return;
		}

		m_Layout.Build(MakeNodes());
		m_Index.clear();
		m_Index.reserve(m_List.size());
		for (size_t i = 0; i < m_List.size(); i++)
		{
			m_Index[m_List[i]] = (uint32_t)i;
		}
		m_Dirty = false;
	}

	int32_t SceneGraph::GetIndex(entt::entity handle)
	{
		Ensure();
		const auto found = m_Index.find(handle);
		return found != m_Index.end() ? (int32_t)found->second : -1;
	}

	void SceneGraph::SetParentUUID(entt::entity handle, UUID parent)
	{
		if ((uint64_t)parent == 0)
		{
			if (m_Scene.m_Registry.has<RelationshipComponent>(handle))
			{
				m_Scene.m_Registry.remove<RelationshipComponent>(handle);
			}
		}
		else
		{
			m_Scene.m_Registry.emplace_or_replace<RelationshipComponent>(handle, parent);
		}
		m_Dirty = true;
	}

	void SceneGraph::MoveSubtree(size_t index, size_t insertAt)
	{
		SceneHierarchy::MoveRange(m_List, index, m_Layout.GetSubtreeEnd(index), insertAt);
		m_Dirty = true;
	}

	void SceneGraph::CheckAfterEdit()
	{
#ifdef HY_DEBUG
		Validate();
#endif
	}

	bool SceneGraph::SetParent(Entity child, Entity parent, bool keepWorldTransform)
	{
		if ((bool)child == false || m_Scene.m_Registry.valid(child) == false)
		{
			return false;
		}

		const int32_t childIndex = GetIndex((entt::entity)child);
		if (childIndex < 0)
		{
			HY_CORE_ERROR("Scene '{}': SetParent on an entity that isn't in the scene's list", m_Scene.GetName());
			return false;
		}

		int32_t parentIndex = -1;
		UUID parentUUID = 0;
		if ((bool)parent)
		{
			parentIndex = m_Scene.m_Registry.valid(parent) ? GetIndex((entt::entity)parent) : -1;
			if (parentIndex < 0)
			{
				HY_CORE_ERROR("Scene '{}': '{}' can't get a parent that isn't in the scene's list", m_Scene.GetName(), child.GetName());
				return false;
			}
			if (parentIndex == childIndex || m_Layout.IsAncestor((size_t)childIndex, (size_t)parentIndex))
			{
				HY_CORE_ERROR("Scene '{}': '{}' can't be the parent of '{}', which contains it", m_Scene.GetName(), parent.GetName(),
					child.GetName());
				return false;
			}
			parentUUID = parent.GetUUID();
		}

		UUID currentParent = 0;
		if (m_Scene.m_Registry.has<RelationshipComponent>(child))
		{
			currentParent = m_Scene.m_Registry.get<RelationshipComponent>(child).Parent;
		}
		if (currentParent == parentUUID)
		{
			return true;
		}

		const bool keepWorld = keepWorldTransform && m_Scene.m_Registry.has<TransformComponent>(child);
		const glm::mat4 world = keepWorld ? m_Scene.GetWorldTransform(child) : glm::mat4(1.0f);

		const size_t insertAt = m_Layout.GetLastChildInsertIndex(parentIndex);
		SetParentUUID(child, parentUUID);
		MoveSubtree((size_t)childIndex, insertAt);
		if (keepWorld)
		{
			m_Scene.SetWorldTransform(child, world);
		}
		CheckAfterEdit();
		return true;
	}

	Entity SceneGraph::GetParent(Entity entity)
	{
		if (entity.HasComponent<RelationshipComponent>() == false)
		{
			return {};
		}
		return m_Scene.GetEntityByUUID(entity.GetComponent<RelationshipComponent>().Parent);
	}

	void SceneGraph::GetChildren(Entity entity, std::vector<Entity>& children)
	{
		children.clear();
		const int32_t index = (bool)entity ? GetIndex((entt::entity)entity) : -1;
		if (index < 0)
		{
			return;
		}

		std::vector<size_t> indices;
		m_Layout.GetChildIndices((size_t)index, indices);
		for (size_t child : indices)
		{
			children.push_back(Entity{ m_List[child], &m_Scene });
		}
	}

	bool SceneGraph::HasChildren(Entity entity)
	{
		const int32_t index = (bool)entity ? GetIndex((entt::entity)entity) : -1;
		return index >= 0 && m_Layout.HasChildren((size_t)index);
	}

	void SceneGraph::GetSubtree(Entity entity, std::vector<Entity>& subtree)
	{
		subtree.clear();
		const int32_t index = (bool)entity ? GetIndex((entt::entity)entity) : -1;
		if (index < 0)
		{
			return;
		}

		for (size_t i = (size_t)index; i < m_Layout.GetSubtreeEnd((size_t)index); i++)
		{
			subtree.push_back(Entity{ m_List[i], &m_Scene });
		}
	}

	bool SceneGraph::IsAncestorOf(Entity ancestor, Entity entity)
	{
		if ((bool)ancestor == false || (bool)entity == false)
		{
			return false;
		}

		const int32_t ancestorIndex = GetIndex((entt::entity)ancestor);
		const int32_t entityIndex = GetIndex((entt::entity)entity);
		return ancestorIndex >= 0 && entityIndex >= 0 && m_Layout.IsAncestor((size_t)ancestorIndex, (size_t)entityIndex);
	}

	uint32_t SceneGraph::GetDepth(Entity entity)
	{
		const int32_t index = (bool)entity ? GetIndex((entt::entity)entity) : -1;
		return index >= 0 ? m_Layout.GetDepth((size_t)index) : 0;
	}

	void SceneGraph::Repair()
	{
		GetOrder();
		std::vector<SceneHierarchy::Node> nodes = MakeNodes();
		const std::vector<SceneHierarchy::Node> before = nodes;

		std::vector<SceneHierarchy::Issue> fixed;
		const std::vector<size_t> order = SceneHierarchy::Repair(nodes, fixed);

		for (const SceneHierarchy::Issue& issue : fixed)
		{
			const entt::entity handle = m_List[issue.Index];
			HY_CORE_WARN("Scene '{}': '{}' is a root now: {} (parent {})", m_Scene.GetName(),
				m_Scene.m_Registry.get<TagComponent>(handle).Tag,
				SceneHierarchy::ProblemToString(issue.Kind), (uint64_t)before[issue.Index].Parent);
			SetParentUUID(handle, nodes[issue.Index].Parent);
		}

		bool reordered = false;
		std::vector<entt::entity> repaired;
		repaired.reserve(order.size());
		for (size_t i = 0; i < order.size(); i++)
		{
			repaired.push_back(m_List[order[i]]);
			if (order[i] != i)
			{
				reordered = true;
			}
		}
		if (reordered)
		{
			HY_CORE_WARN("Scene '{}': entities were reordered so every parent comes before its children", m_Scene.GetName());
			m_List = std::move(repaired);
			m_Dirty = true;
		}
		CheckAfterEdit();
	}

	bool SceneGraph::Validate()
	{
		GetOrder();
		std::vector<SceneHierarchy::Issue> issues;
		if (SceneHierarchy::Validate(MakeNodes(), &issues))
		{
			return true;
		}

		for (const SceneHierarchy::Issue& issue : issues)
		{
			HY_CORE_ERROR("Scene '{}': hierarchy broken at '{}': {}", m_Scene.GetName(),
				m_Scene.m_Registry.get<TagComponent>(m_List[issue.Index]).Tag,
				SceneHierarchy::ProblemToString(issue.Kind));
		}
		return false;
	}
}
