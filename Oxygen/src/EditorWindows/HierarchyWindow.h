#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"

#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// The scene's entities as a tree (Roadmap S3): each row indented by its depth, with an arrow to fold its
	// children. Drag a row onto the middle of another to make it a child, onto its top or bottom edge to place it
	// before or after as a sibling, onto "Root" to make it a root; children keep their place in the world.
	// Prefabs from the Project window can be dropped onto "Root" or onto a row (as its child).
	class HierarchyWindow
	{
	public:
		HierarchyWindow();
		~HierarchyWindow();
		void OnImguiAttach();
		void OnImguiRender(Ref<Scene> scene);

		Entity GetSelectedEntity() const { return m_SelectedEntity; }
		void ClearSelection() { m_SelectedEntity = {}; }
		void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

		// Called with the entity of a row that was double-clicked (it is selected too). The editor uses
		// it to move the scene camera to the entity.
		std::function<void(Entity)> OnEntityFocus;

	private:
		// What the user asked for during the frame's loop: applied after it, since the list must not change
		// while it is drawn.
		struct PendingEdit
		{
			enum class Type : uint8_t { None = 0, MoveBefore, MoveAfter, MakeChild, MakeRoot, Duplicate, Delete, CreateChild, InstantiatePrefab };
			enum class Template : uint8_t { Empty = 0, Sprite, Light };

			Type Kind = Type::None;
			Entity Subject;   // the entity moved, duplicated or deleted
			Entity Target;    // the row it was dropped on, or the parent to create under (empty: a root)
			Template Create = Template::Empty;
			std::filesystem::path Prefab;

			// Move, reparent, duplicate, delete: `subject`, and the row it was dropped on.
			static PendingEdit Of(Type kind, Entity subject, Entity target = {})
			{
				PendingEdit edit;
				edit.Kind = kind;
				edit.Subject = subject;
				edit.Target = target;
				return edit;
			}
			// A new entity from a template, under `parent` (empty: a root).
			static PendingEdit Creating(Template kind, Entity parent)
			{
				PendingEdit edit = Of(Type::CreateChild, {}, parent);
				edit.Create = kind;
				return edit;
			}
			// An instance of the prefab at `path`, under `parent` (empty: a root).
			static PendingEdit Instantiating(const std::filesystem::path& path, Entity parent)
			{
				PendingEdit edit = Of(Type::InstantiatePrefab, {}, parent);
				edit.Prefab = path;
				return edit;
			}
		};

		void DrawRow(Scene& scene, Entity entity, bool searching, bool& open);
		void DrawRowDropTarget(Scene& scene, Entity entity);
		void DrawRowContextMenu(Scene& scene, Entity entity);
		void ApplyPendingEdit(Scene& scene);
		Entity CreateFromTemplate(Scene& scene, PendingEdit::Template kind, Entity parent);

		void SaveAsPrefab(Entity entity);
		// True when the entity's name contains the search text (case-insensitive); an empty search matches all.
		bool MatchesSearch(const std::string& name) const;

	private:
		Entity m_SelectedEntity;
		PendingEdit m_Pending;
		char m_SearchBuffer[128] = {};
	};
}
