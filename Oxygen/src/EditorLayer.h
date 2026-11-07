#pragma once
#include "Hydro.h"
#include "EditorContext.h"
#include "EditorLaunch.h"
#include "EditorMenuBar.h"
#include "EditorCommands/EditWatcher.h"
#include "Viewports/EditorViewport.h"
#include "Viewports/GameViewport.h"
#include "EditorWindows/HierarchyWindow.h"
#include "EditorWindows/StatsWindow.h"
#include "EditorWindows/ConsoleWindow.h"
#include "EditorWindows/InspectorWindow.h"
#include "EditorWindows/ProjectWindow.h"
#include "EditorWindows/RenderSettingsWindow.h"
#include "EditorWindows/ProjectSettingsWindow.h"
#include "EditorWindows/BuildWindow.h"
#include "EditorWindows/EditorSettingsWindow.h"
#include "EditorWindows/SpriteEditorWindow.h"
#include "EditorWindows/SpriteAnimationWindow.h"
#include "EditorWindows/ProjectHubWindow.h"
#include "RecentProjects.h"

namespace Hydro
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer()
			: EditorLayer(EditorLaunchOptions{})
		{
		}
		explicit EditorLayer(const EditorLaunchOptions& launch)
			: m_Launch(launch, m_Context, m_GameViewport)
		{
		}
		virtual ~EditorLayer() {};

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		// Projects (.hyproj). Opening one applies its render settings and opens its start scene.
		// <location>/<name>/<name>.hyproj with the standard folders; generic until it uses a game type.
		void NewProject(const std::string& name, const std::filesystem::path& location);
		bool OpenProject();
		void OpenProject(const std::filesystem::path& path);
		// Copies the live render settings into the project, then writes the .hyproj.
		void SaveProject();
		// Saves the session, closes the project and shows the hub.
		void CloseProject();
		// The open project goes to the top of the hub's recent list (saved).
		void RememberProject();
		// At startup: the last project and scene (Editor Settings > Startup); without one the hub shows.
		void OpenStartupProject();

		// Editor settings (EditorSettings.yaml): loading applies them to ImGui and the editor camera.
		void LoadEditorSettings();
		void SaveEditorSettings();
		// The session (user file) and the open project's editor state (its last scene).
		void SaveSession();
		// Revert in the Editor Settings window: reload the file, but keep what is open now.
		void RevertEditorSettings();
		// Saves the open scene every Autosave.IntervalMinutes while editing.
		void UpdateAutosave(float deltaTime);

		// The View menu's windows: the menu, the session and the restore all use this one list.
		std::vector<EditorMenuBar::ViewWindow> GetViewWindows();
		// Opens and closes the View menu windows as the session left them.
		void RestoreOpenWindows();
		// Copies the windows' open state into the session; returns true when it changed.
		bool StoreOpenWindows();

		// Scenes (.hyscene). These work on the editor scene and stop play mode first. New Scene copies the project's
		// scene template when it has one; otherwise, and for New Empty Scene, the scene has only a camera.
		void NewScene();
		void NewEmptyScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();
		void SetEditorScene(const Ref<Scene>& scene);

		// Play runs a copy of the editor scene; Stop throws the copy away, so playing never changes the editor scene.
		void PlayScene();
		void StopScene();
		// Runs the Push or Pop a script asked for through SceneTransition. Called after the frame's update.
		void ProcessSceneTransition();
		// Selects the entity in `scene` with the same UUID as `previous`, or clears the selection. Keeps it across Play/Stop.
		void SelectSameEntity(Entity previous, Scene& scene);
		bool IsPlaying() const { return m_Context.Playing; }

		// A prefab or scene dropped from the Project window onto the Editor or Game window.
		void OnAssetDropped(const std::filesystem::path& path);
		// The prefab's instance in the active scene, selected, as one undo step.
		Entity InstantiatePrefab(const std::filesystem::path& path);
		// A path renamed in the Project window: the open scene, the session scene, the inspected asset and the sprite
		// windows' files follow it.
		void OnPathRenamed(const std::filesystem::path& from, const std::filesystem::path& to);
		// A file was deleted in the Project window: whatever shows or edits it lets go.
		void OnPathDeleted(const std::filesystem::path& path);

		// Asset editors. Images open in the Sprite Editor, .hyanim files in the Animation window.
		void OpenAsset(const std::filesystem::path& path);
		// An image's .hymeta was saved: sprites and animation sets that use it get the re-imported texture (same handle),
		// keeping each sprite by name. Called before the saving window reads the image again.
		void OnTextureReimported(const std::filesystem::path& imagePath);
		// A .hyanim was saved: animators that play it get the new set.
		void OnAnimationSetSaved(const std::filesystem::path& path);

		// A new scene starts with a primary camera framing the origin.
		void CreateDefaultCamera();

	private:
		// The editor's shared state, which the windows and the viewports below are given.
		EditorContext m_Context;
		EditorViewport m_EditorViewport{ m_Context };
		GameViewport m_GameViewport{ m_Context };
		EditorMenuBar m_MenuBar{ m_Context };
		EditWatcher m_EditWatcher;
		// What the command line asked for (unattended runs).
		EditorLaunch m_Launch;
		SceneStack m_GameScenes; // the scenes that run while playing (a copy of the editor scene at the bottom); empty in Edit
		Scope<SceneRenderer> m_SceneRenderer;

		Hydro::Ref<HierarchyWindow> m_HierarchyWindow;
		Hydro::Ref<StatsWindow> m_StatsWindow;
		Hydro::Ref<ConsoleWindow> m_ConsoleWindow;
		Hydro::Ref<InspectorWindow> m_InspectorWindow;
		Hydro::Ref<ProjectWindow> m_ProjectWindow;
		Hydro::Ref<RenderSettingsWindow> m_RenderSettingsWindow;
		Hydro::Ref<ProjectSettingsWindow> m_ProjectSettingsWindow;
		Hydro::Ref<BuildWindow> m_BuildWindow;
		Hydro::Ref<EditorSettingsWindow> m_EditorSettingsWindow;
		Hydro::Ref<SpriteEditorWindow> m_SpriteEditorWindow;
		Hydro::Ref<SpriteAnimationWindow> m_SpriteAnimationWindow;
		Hydro::Ref<ProjectHubWindow> m_ProjectHub;
		RecentProjects m_RecentProjects;
		entt::entity m_LastSelectedEntity = entt::null;
		float m_AutosaveTimer = 0.0f; // seconds since the last save
	};
}

