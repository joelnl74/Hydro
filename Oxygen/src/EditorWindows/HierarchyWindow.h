#pragma once
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Core/Base.h"
#include "EditorContext.h"

#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// The scene's entities as a foldable tree. Drag a row onto another to parent it or onto its edge to reorder;
	// children keep their place in the world. Prefabs can be dropped onto a row or onto "Root".
	class HierarchyWindow
	{
	public:
		explicit HierarchyWindow(EditorContext& context);
		~HierarchyWindow();
		void OnImguiAttach();
		void OnImguiRender(Ref<Scene> scene);

		// Called with a double-clicked row's entity (it is selected too); the editor frames the camera on it.
		std::function<void(Entity)> OnEntityFocus;

	private:
		// An edit asked for while drawing, applied afterwards because the list must not change mid-loop.
		struct PendingEdit
		{
			enum class Type : uint8_t { None = 0, MoveBefore, MoveAfter, MakeChild, MakeRoot, Duplicate, Delete, CreateChild,
				InstantiatePrefab };
			enum class Template : uint8_t { Empty = 0, Sprite, Light, UICanvas, UIPanel, UIImage, UIText, UIButton, UIList };

			Type Kind = Type::None;
			Entity Subject;   // the entity moved, duplicated or deleted
			Entity Target;    // the row it was dropped on, or the parent to create under (empty: a root)
			Template Create = Template::Empty;
			std::filesystem::path Prefab;

			// Move, reparent, duplicate or delete `subject` relative to the row it was dropped on.
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
		// A UI canvas, or an element under `parent`; an element without a canvas above it gets a new root canvas.
		Entity CreateUIFromTemplate(Scene& scene, PendingEdit::Template kind, Entity parent);
		// The Create > UI menu items, creating under `parent` (empty: a root).
		void DrawCreateUIMenu(Entity parent);

		void SaveAsPrefab(Entity entity);
		// True when the entity's name contains the search text (case-insensitive); an empty search matches all.
		bool MatchesSearch(const std::string& name) const;

	private:
		// Its Selection is the selected row.
		EditorContext& m_Context;
		PendingEdit m_Pending;
		char m_SearchBuffer[128] = {};
	};
}
