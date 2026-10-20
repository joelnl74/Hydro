#pragma once
#include "Hydro.h"
#include "EditorWindows/HierachyWindow.h"
#include "EditorWindows/StatsWindow.h"
#include "EditorWindows/ConsoleWindow.h"
#include "EditorWindows/InspectorWindow.h"
#include "EditorWindows/ProjectWindow.h"
#include "EditorWindows/RenderSettingsWindow.h"
#include "EditorWindows/ProjectSettingsWindow.h"
#include "EditorWindows/EditorSettingsWindow.h"
#include "EditorSettings.h"

#include <glm/glm.hpp>

namespace Hydro
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer() {};
		virtual ~EditorLayer() {};

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

	private:
		// Projects (.hyproj, YAML). Opening one applies its render settings and opens its start scene.
		void NewProject();
		bool OpenProject();
		void OpenProject(const std::filesystem::path& path);
		// Copies the live render settings into the project, then writes the .hyproj.
		void SaveProject();
		// Opens the first .hyproj in EditorPaths::GetDefaultProjectDirectory(), or starts an unsaved one there.
		void OpenDefaultProject();
		// At startup: the last project and scene (Editor Settings > Startup), else the default project.
		void OpenStartupProject();

		// Editor settings (EditorSettings.yaml): loading applies them to ImGui and the editor camera.
		void LoadEditorSettings();
		void SaveEditorSettings();
		// Revert in the Editor Settings window: reload the file, but keep what is open now.
		void RevertEditorSettings();
		// Saves the open scene every Autosave.IntervalMinutes while editing.
		void UpdateAutosave(float deltaTime);

		// The windows in the View menu, by menu name: the menu, the session and the restore all use
		// this one list, so a window added here is remembered too.
		struct ViewWindow
		{
			const char* Name;
			bool* Open;
		};
		std::vector<ViewWindow> GetViewWindows();
		// Opens / closes the View menu windows as the session left them (windows not listed keep their default).
		void RestoreOpenWindows();
		// Copies the windows' open state into the session; returns true when it changed.
		bool StoreOpenWindows();

		// Scenes (.hyscene, YAML). These always work on the editor scene and stop play mode first.
		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();
		void SetEditorScene(const Ref<Scene>& scene);

		// Play mode. Play runs a copy of the editor scene (Scene::Copy) with OnUpdateRuntime; Stop
		// throws the copy away, so nothing done while playing changes the editor scene.
		void PlayScene();
		void StopScene();
		bool IsPlaying() const { return m_SceneState == SceneState::Play; }

		// Prefab / scene dropped from the project window onto the editor or game window.
		void OnAssetDropped(const std::filesystem::path& path);
		// A folder or scene was renamed in the project window: moves the open scene's path (and its
		// name), the last session scene and the project's start scene if they were at or inside it.
		void OnPathRenamed(const std::filesystem::path& from, const std::filesystem::path& to);
		void AcceptAssetDrop();

		// The Play / Pause / Step buttons, centred in the main menu bar.
		void DrawPlayControls();
		// Editor window: the active scene through the editor camera, which it controls.
		void DrawEditorWindow();
		// Game window: the active scene through its primary camera, as the game will show it.
		void DrawGameWindow();
		// A new scene starts with a primary camera framing the origin.
		void CreateDefaultCamera();

		// Demo scene: animated spiral galaxy, big enough to need several batches. The stars are raw
		// registry entities (not saved, not copied by Scene::Copy), so PlayScene copies them itself.
		void CreateGalaxy();
		void UpdateGalaxy(Scene& scene, float deltaTime);
		static void CopyGalaxyStars(Scene& source, Scene& destination);

	private:
		enum class SceneState { Edit, Play };

		Hydro::Ref<Scene> m_EditorScene;  // what is edited and saved
		Hydro::Ref<Scene> m_RuntimeScene; // the copy that runs while playing; null in Edit
		Hydro::Ref<Scene> m_ActiveScene;  // m_RuntimeScene while playing, else m_EditorScene
		Hydro::Scope<SceneRenderer> m_SceneRenderer;
		std::filesystem::path m_EditorScenePath; // empty until the scene is saved / opened

		SceneState m_SceneState = SceneState::Edit;
		bool m_Paused = false;
		uint32_t m_StepFrames = 0; // runtime frames still to run while paused (Step button)

		// Editor Windows.
		Hydro::Ref<HierachyWindow> m_HierarchyWindow;
		Hydro::Ref<StatsWindow> m_StatsWindow;
		Hydro::Ref<ConsoleWindow> m_ConsoleWindow;
		Hydro::Ref<InspectorWindow> m_InspectorWindow;
		Hydro::Ref<ProjectWindow> m_ProjectWindow;
		Hydro::Ref<RenderSettingsWindow> m_RenderSettingsWindow;
		Hydro::Ref<ProjectSettingsWindow> m_ProjectSettingsWindow;
		Hydro::Ref<EditorSettingsWindow> m_EditorSettingsWindow;

		// How the editor looks and behaves; loaded from EditorSettings.yaml in OnAttach. The session
		// part (last project / scene) is kept up to date here and written on close.
		EditorSettings m_EditorSettings;
		float m_AutosaveTimer = 0.0f; // seconds since the last save

		// The two pictures of the scene. Only windows that are visible are rendered.
		Hydro::Scope<RenderView> m_EditorView;
		Hydro::Scope<RenderView> m_GameView;
		EditorCamera m_EditorCamera;
		bool m_EditorViewVisible = false;
		bool m_EditorViewHovered = false;
		bool m_GameViewVisible = false;
		bool m_SceneHasCamera = true; // false when the game view found no primary camera last frame
		float m_Time = 0.0f;
	};
}

