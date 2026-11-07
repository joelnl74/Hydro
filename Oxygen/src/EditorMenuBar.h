#pragma once
#include "EditorContext.h"
#include "RecentProjects.h"

#include <functional>
#include <vector>

namespace Hydro
{
	// The main menu bar: File, Edit, View, Create, the Play / Pause / Step buttons and the waterline, and the undo keys.
	// Opening, saving and playing are EditorLayer's: the menu reaches them through the callbacks.
	class EditorMenuBar
	{
	public:
		// One window of the View menu: its name (the session remembers it by this) and its open flag.
		struct ViewWindow
		{
			const char* Name;
			bool* Open;
		};

		std::function<void()> OnNewProject;
		std::function<void()> OnOpenProject;
		std::function<void(const std::filesystem::path&)> OnOpenRecentProject;
		std::function<void()> OnCloseProject;
		std::function<void()> OnProjectHub;
		std::function<void()> OnSaveProject;
		std::function<void()> OnBuild;
		std::function<void()> OnNewScene;
		std::function<void()> OnNewEmptyScene;
		std::function<void()> OnOpenScene;
		std::function<void()> OnSaveScene;
		std::function<void()> OnSaveSceneAs;
		std::function<void()> OnPlay;
		std::function<void()> OnStop;

		explicit EditorMenuBar(EditorContext& context);

		// File > Recent Projects lists these (the hub's list, owned by EditorLayer).
		void SetRecentProjects(const RecentProjects* recent) { m_RecentProjects = recent; }

		// Inside a window with ImGuiWindowFlags_MenuBar (the dockspace).
		void Draw(const std::vector<ViewWindow>& viewWindows);
		// Ctrl+Z undoes, Ctrl+Y and Ctrl+Shift+Z redo (edit mode, not while typing); the Edit menu does the same, and
		// duplicates and deletes the selection (Ctrl+D and Delete are the hierarchy's and the Editor view's).
		void UpdateKeys();

	private:
		void DrawFileMenu();
		void DrawEditMenu();
		void DrawCreateMenu();
		// The Play / Pause / Step buttons, centred in the bar.
		void DrawPlayControls();
		// A brush stroke in progress becomes a step first, so it is what is undone. The selection follows the step.
		void Undo();
		void Redo();
		void FollowSelection(const EditorCommand* command, bool afterEdit, UUID previous);
		// The scene's name at the bar's right end, with " *" while it has unsaved changes.
		void DrawSceneName();

	private:
		EditorContext& m_Context;
		const RecentProjects* m_RecentProjects = nullptr;
	};
}
