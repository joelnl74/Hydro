#pragma once
#include "Hydro/Core/UUID.h"
#include "SceneHierarchy.h"

#include <entt.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	class Entity;
	class Scene;

	// A scene's entity list, kept as its hierarchy in depth-first order (each parent directly followed by its subtree),
	// and the parent links (RelationshipComponent) that shape it. Scene owns one and forwards its hierarchy API here.
	class SceneGraph
	{
	public:
		explicit SceneGraph(Scene& scene);
		~SceneGraph();
		SceneGraph(const SceneGraph&) = delete;
		SceneGraph& operator=(const SceneGraph&) = delete;

		// A new entity: last in the list, as a root.
		void Add(entt::entity handle);
		// An entity that is being destroyed (its children are handled by the caller).
		void Remove(entt::entity handle);
		// The parent links or the list changed outside the graph (copies remapped their parents).
		void MarkDirty() { m_Dirty = true; }

		// The list after dropping entities destroyed through the registry and picking up ones that got an IDComponent
		// without CreateEntity. Scans the registry only after an IDComponent came or went, so per-row calls stay cheap.
		const std::vector<entt::entity>& GetOrder();
		// Brings the list and its layout up to date. GetList, GetLayout and IsListed are as of the last call.
		void Ensure();
		const std::vector<entt::entity>& GetList() const { return m_List; }
		const SceneHierarchy& GetLayout() const { return m_Layout; }
		bool IsListed(entt::entity handle) const { return m_Index.find(handle) != m_Index.end(); }
		// The entity's index in the list, or -1 when it isn't listed (ensures first).
		int32_t GetIndex(entt::entity handle);

		bool SetParent(Entity child, Entity parent, bool keepWorldTransform);
		void MoveEntity(Entity entity, Entity target, bool placeAfter);
		// Moves the subtree at `index` to `insertAt`, an index from before the move (SceneHierarchy::GetLastChildInsertIndex
		// / GetSiblingInsertIndex).
		void MoveSubtree(size_t index, size_t insertAt);
		Entity GetParent(Entity entity);
		void GetChildren(Entity entity, std::vector<Entity>& children);
		bool HasChildren(Entity entity);
		void GetSubtree(Entity entity, std::vector<Entity>& subtree);
		bool IsAncestorOf(Entity ancestor, Entity entity);
		uint32_t GetDepth(Entity entity);

		void Repair();
		bool Validate();
		// Debug builds: Validate. Release: nothing.
		void CheckAfterEdit();

	private:
		std::vector<SceneHierarchy::Node> MakeNodes();
		// Parent 0 removes the RelationshipComponent; nothing moves in the list.
		void SetParentUUID(entt::entity handle, UUID parent);
		// Registry signal: an IDComponent was added or removed, so the next GetOrder compares the list with the registry.
		void OnIDComponentChanged(entt::registry& registry, entt::entity handle);

	private:
		Scene& m_Scene;
		std::vector<entt::entity> m_List;
		// m_Layout lays out m_List; rebuilt with m_Index on first use after a change (m_Dirty).
		SceneHierarchy m_Layout;
		std::unordered_map<entt::entity, uint32_t> m_Index;
		bool m_Dirty = true;
		// An IDComponent came or went since GetOrder last compared m_List with the registry.
		bool m_ListStale = true;
	};
}
