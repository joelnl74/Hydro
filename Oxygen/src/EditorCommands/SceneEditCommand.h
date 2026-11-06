#pragma once
#include "EditorCommand.h"

#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Scene.h>

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Hydro
{
	// One entity at one moment, for SceneEditCommand: absent, or its name and components (the YAML EntitySerializer
	// writes, without the parent) with its parent and its place among its siblings.
	struct EntityState
	{
		UUID Id = UUID(0);
		bool Exists = false;
		std::string Components;
		UUID Parent = UUID(0);
		uint32_t SiblingIndex = 0; // among the parent's children, or among the roots
		uint32_t Depth = 0;        // only orders the restore (parents first)

		static EntityState Capture(Scene& scene, UUID id);
		// The YAML Capture keeps for an entity.
		static std::string SerializeComponents(Entity entity);

		bool operator==(const EntityState& other) const
		{
			return Id == other.Id && Exists == other.Exists && Components == other.Components && Parent == other.Parent
				&& SiblingIndex == other.SiblingIndex;
		}
		bool operator!=(const EntityState& other) const { return (*this == other) == false; }
	};

	// Any change to a set of entities: their states before and after. Undo applies the before states and redo the
	// after states. This covers edits, create, delete, duplicate, move and reparent alike.
	class SceneEditCommand : public EditorCommand
	{
	public:
		SceneEditCommand(const Ref<Scene>& scene, std::string name, std::vector<EntityState> before, std::vector<EntityState> after,
			UUID selectionBefore, UUID selectionAfter);

		// Nothing changed: don't push it.
		bool IsEmpty() const { return m_Before == m_After; }

		void Undo() override { Apply(*m_Scene, m_Before); }
		void Redo() override { Apply(*m_Scene, m_After); }
		const char* GetName() const override { return m_Name.c_str(); }
		size_t GetMemorySize() const override;
		bool HasSelection() const override { return true; }
		UUID GetSelection(bool afterEdit) const override { return afterEdit ? m_SelectionAfter : m_SelectionBefore; }

		// Makes the scene match `states`: what should be absent is destroyed, what is missing is created with its UUID,
		// components are replaced in place (only where they differ), then parents and places are put back, parents first.
		static void Apply(Scene& scene, const std::vector<EntityState>& states);

	private:
		static void Place(Scene& scene, Entity entity, const EntityState& state);

	private:
		Ref<Scene> m_Scene;
		std::string m_Name;
		std::vector<EntityState> m_Before;
		std::vector<EntityState> m_After;
		UUID m_SelectionBefore = UUID(0);
		UUID m_SelectionAfter = UUID(0);
	};

	// One structural edit as a SceneEditCommand: Capture what it will touch, edit, Finish. Entities that appeared since
	// the scope began count as created. Without a scene (while playing) it records nothing.
	class SceneEditScope
	{
	public:
		SceneEditScope(const Ref<Scene>& scene, std::string name, UUID selectionBefore);

		// The entity (with `subtree`, its descendants too) as it is now, before the edit.
		void Capture(Entity entity, bool subtree = true);
		// The step: the captured entities as they are now, and every entity created since the scope began (absent
		// before). Null when nothing changed.
		Scope<SceneEditCommand> Finish(UUID selectionAfter);

	private:
		void Add(UUID id, bool existedBefore);

	private:
		Ref<Scene> m_Scene;
		std::string m_Name;
		UUID m_SelectionBefore = UUID(0);
		std::vector<EntityState> m_Before;
		std::unordered_set<uint64_t> m_Ids;
		std::unordered_set<uint64_t> m_Existing; // every entity when the scope began
	};
}
