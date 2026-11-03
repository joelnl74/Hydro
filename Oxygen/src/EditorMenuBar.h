#pragma once
#include "EditorContext.h"

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

		// Inside a window with ImGuiWindowFlags_MenuBar (the dockspace).
		void Draw(const std::vector<ViewWindow>& viewWindows);
		// Ctrl+Z undoes, Ctrl+Y and Ctrl+Shift+Z redo (edit mode, not while typing); the Edit menu does the same.
		void UpdateKeys();

	private:
		void DrawFileMenu();
		void DrawEditMenu();
		void DrawCreateMenu();
		// The Play / Pause / Step buttons, centred in the bar.
		void DrawPlayControls();
		// A brush stroke in progress becomes a step first, so it is what is undone.
		void Undo();
		void Redo();

	private:
		EditorContext& m_Context;
	};
}
