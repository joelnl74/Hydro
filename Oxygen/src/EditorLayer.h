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
#include "EditorWindows/SpriteEditorWindow.h"
#include "EditorWindows/SpriteAnimationWindow.h"
#include "EditorSettings.h"

#include <glm/glm.hpp>
#include <imgui/imgui.h>

namespace Hydro
{
	// What Oxygen was started with (OxygenEditorApplication parses the command line), so a scene can be
	// opened, played, captured and checked without anyone at the keyboard:
	//
	//   Oxygen [--project <file.hyproj>] [--scene <name | file.hyscene>] [--play] [--frames <n>]
	//          [--capture <file.png>] [--validate] [--exit]
	//
	// --project opens that project instead of the last one; --scene a scene of it (a name looks in its
	// Assets/Scenes). --play presses Play. After --frames frames (120 by default) --capture saves the Game
	// view (1280 x 720) to that file, and --exit closes the editor (--capture and --validate imply it).
	// --validate loads every scene of the project, logs what is missing, and exits with code 1 when any
	// warned. All of it is logged (logs/Hydro.log and the console).
	struct EditorLaunchOptions
	{
		std::filesystem::path Project;
		std::string Scene;
		bool Play = false;
		uint32_t Frames = 120;
		std::filesystem::path Capture;
		bool Validate = false;
		bool Exit = false;

		static EditorLaunchOptions Parse(const ApplicationCommandLine& commandLine);
		bool IsAutomated() const { return Capture.empty() == false || Validate || Exit; }
	};

	class EditorLayer : public Layer
	{
	public:
		EditorLayer() {};
		explicit EditorLayer(const EditorLaunchOptions& launch)
			: m_Launch(launch) {
		}
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
		// The command line's project and scene, after the startup project; then --validate or --play.
		void ApplyLaunchOptions();
		// A scene by name (in the project's Assets/Scenes) or path; empty when there is none.
		std::filesystem::path FindScene(const std::string& nameOrPath) const;
		// --validate: loads every scene of the project into a scratch scene; true when none warned.
		bool ValidateProject();
		// Counts frames for --frames, then captures (--capture) and quits (--exit).
		void UpdateLaunchAutomation();

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
		// Runs the Push / Pop a script asked for through SceneTransition (see there): the running scene goes
		// on a stack, or is replaced by the one below it. Called after the frame's update.
		void ProcessSceneTransition();
		// Selects, in `scene`, the entity with the same UUID as `previous` (an entity of the scene being
		// left); clears the selection when there is none. Play and Stop use it to keep the selection.
		void SelectSameEntity(Entity previous, Scene& scene);
		bool IsPlaying() const { return m_SceneState == SceneState::Play; }

		// Prefab / scene dropped from the project window onto the editor or game window.
		void OnAssetDropped(const std::filesystem::path& path);
		// A folder or asset was renamed in the Project window: moves the open scene's path (and its name),
		// the last session scene, the inspected asset and the sprite windows' files if they were at or inside it.
		void OnPathRenamed(const std::filesystem::path& from, const std::filesystem::path& to);
		// A file was deleted in the Project window: whatever shows or edits it lets go.
		void OnPathDeleted(const std::filesystem::path& path);
		void AcceptAssetDrop();

		// Asset editors. Images open in the Sprite Editor, .hyanim files in the Animation window.
		void OpenAsset(const std::filesystem::path& path);
		// An image's .hymeta was saved: sprites (editor and play scene) and the animation sets that
		// use the image get the re-imported texture (the AssetManager reloads it into the same handle), keeping
		// each sprite by name. Called before the window that saved the file reads the image again.
		void OnTextureReimported(const std::filesystem::path& imagePath);
		// A .hyanim was saved: animators that play it get the new set.
		void OnAnimationSetSaved(const std::filesystem::path& path);

		// The Play / Pause / Step buttons, centred in the main menu bar.
		void DrawPlayControls();
		// Editor window: the active scene through the editor camera, which it controls.
		void DrawEditorWindow();
		// The Editor window's Camera menu (2D / 3D, the standard views, the controls) and the 2D/3D switch.
		void DrawEditorCameraMenu();
		// Over the Editor window's image: the fly speed while the camera flies.
		void DrawEditorCameraHint(const ImVec2& imageMin);
		// Game window: the active scene through its primary camera, as the game will show it.
		void DrawGameWindow();
		// Game full screen (F11, or Full screen in the Game window's menu bar): the game view covers the
		// editor's whole window, drawn over everything else, and with PlayModeSettings::FullscreenCoversMonitor
		// the editor's window covers the monitor too. F11 or Escape leaves it.
		void SetGameFullscreen(bool fullscreen);
		void DrawGameFullscreen();
		void UpdateFullscreenKeys();
		// Sizes a view (and the scene's or editor camera's viewport) to its window. A capture asked for
		// with the Capture menu or F9 / Shift+F9 is requested here, at the fixed capture size if that
		// option is on (the window shows that one frame stretched).
		void ResizeGameView(uint32_t width, uint32_t height);
		void ResizeEditorView(uint32_t width, uint32_t height);
		// The Capture menu, shown in the Editor and Game windows' menu bars.
		void DrawCaptureMenu();
		void UpdateCaptureKeys();
		// <project>/.hydro/captures/<scene>_<view>_<yyyymmdd-hhmmss-mmm>.png
		std::filesystem::path MakeCapturePath(const std::string& viewName) const;
		// Moves the editor camera to the entity and zooms to frame it (the selection outline shows which).
		void FocusEntity(Entity entity);
		// Click-to-select: a left click in the Editor view's image (at `imageMin`, `imageSize` ImGui pixels)
		// asks the view for the entity under it; the answer, a frame or two later, selects it (or clears).
		void UpdateClickSelection(const ImVec2& imageMin, const ImVec2& imageSize);
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
		std::vector<Hydro::Ref<Scene>> m_SuspendedScenes; // scenes under the running one (SceneTransition::Push), bottom first
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
		Hydro::Ref<SpriteEditorWindow> m_SpriteEditorWindow;
		Hydro::Ref<SpriteAnimationWindow> m_SpriteAnimationWindow;
		// The asset the inspector shows while no entity is selected (picked in the Project window).
		std::filesystem::path m_InspectedAsset;
		entt::entity m_LastSelectedEntity = entt::null;

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
		// Editor window overlays (this session only): outline the selected entity / every collider.
		bool m_ShowSelectionOutline = true;
		bool m_ShowColliders = false;
		bool m_FocusKeyDown = false; // F pressed last frame, so holding it focuses once
		// Click-to-select in the Editor view (M7): a left press in the image, a release without moving.
		bool m_ClickPending = false;
		ImVec2 m_ClickStart{ 0.0f, 0.0f };
		bool m_GameViewVisible = false;
		bool m_SceneHasCamera = true; // false when the game view found no primary camera last frame
		bool m_GameFullscreen = false;
		bool m_FocusFullscreen = false;        // bring the full screen view to the front on its first frame
		bool m_WindowFullscreenByGame = false; // the editor's window went full screen for the game, and goes back with it
		int m_PendingWindowFullscreen = -1;    // -1 nothing, 0 / 1: change the window in OnUpdate, outside the ImGui frame
		bool m_FullscreenKeyDown = false;      // F11 last frame, so holding it toggles once
		bool m_EscapeKeyDown = false;
		float m_FullscreenHintTime = 0.0f;     // seconds left to show how to leave full screen
		// View captures (this session only). A request waits until its view is drawn.
		static constexpr uint32_t CaptureWidth = 1280;
		static constexpr uint32_t CaptureHeight = 720;
		bool m_CaptureFixedSize = true;        // render the captured frame at CaptureWidth x CaptureHeight
		bool m_CaptureGameRequested = false;
		bool m_CaptureEditorRequested = false;
		bool m_CaptureKeyDown = false;         // F9 last frame, so holding it captures once
		float m_Time = 0.0f;

		// Unattended runs (EditorLaunchOptions): what was asked and how far it got.
		enum class LaunchStep : uint8_t { None, Waiting, Capturing, Done };
		EditorLaunchOptions m_Launch;
		LaunchStep m_LaunchStep = LaunchStep::None;
		uint32_t m_LaunchFramesLeft = 0;
	};
}

