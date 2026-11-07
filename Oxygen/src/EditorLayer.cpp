#include "EditorLayer.h"
#include "Viewports/CaptureControls.h"

#include <imgui/imgui.h>
#include <ImGuizmo.h>
#include <string>

#include "Hydro/Utils/PlatformUtils.h"
#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Scene/SpriteAnimation.h"
#include "EditorPaths.h"
#include "Utilities/EditorChrome.h"
#include "Utilities/PathFollower.h"

#include <algorithm>
#include <fstream>
#include <map>

namespace Hydro
{
	// The registered scene that comes first by path, or 0 when the project has none (or only missing ones).
	static AssetHandle FindFirstScene()
	{
		AssetHandle first = 0;
		std::filesystem::path firstPath;
		for (const auto& [handle, metadata] : AssetManager::GetRegistry().GetAll())
		{
			if (metadata.Type != AssetType::Scene || metadata.IsMissing)
			{
				continue;
			}
			if ((uint64_t)first == 0 || metadata.Path < firstPath)
			{
				first = handle;
				firstPath = metadata.Path;
			}
		}
		return first;
	}

	void EditorLayer::OnAttach()
	{
		m_Context.EditorScene = CreateRef<Scene>("Scene", false);
		m_Context.EditorScene->Init();
		m_Context.ActiveScene = m_Context.EditorScene;
		m_SceneRenderer = CreateScope<SceneRenderer>(m_Context.ActiveScene);

		m_EditorViewport.Init();
		m_GameViewport.Init();
		m_Context.OnAssetDropped = [this](const std::filesystem::path& path) { OnAssetDropped(path); };

		m_HierarchyWindow = CreateRef<HierarchyWindow>(m_Context);
		m_StatsWindow = CreateRef<StatsWindow>();
		m_ConsoleWindow = CreateRef<ConsoleWindow>();
		m_InspectorWindow = CreateRef<InspectorWindow>();
		m_ProjectWindow = CreateRef<ProjectWindow>();
		m_ProjectWindow->OnOpenScene = [this](const std::filesystem::path& path) { OpenScene(path); };
		m_ProjectWindow->OnInstantiatePrefab = [this](const std::filesystem::path& path) { InstantiatePrefab(path); };
		m_HierarchyWindow->OnEntityFocus = [this](Entity entity) { m_EditorViewport.FocusEntity(entity); };
		m_ProjectWindow->OnPathRenamed = [this](const std::filesystem::path& from, const std::filesystem::path& to) { OnPathRenamed(from, to); };
		m_ProjectWindow->OnPathDeleted = [this](const std::filesystem::path& path) { OnPathDeleted(path); };
		m_ProjectWindow->OnAssetSelected = [this](const std::filesystem::path& path)
		{
			// The inspector shows the asset instead of an entity.
			m_Context.InspectedAsset = path;
			m_Context.Selection = {};
		};
		m_ProjectWindow->OnOpenAsset = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_InspectorWindow->OnTextureReimported = [this](const std::filesystem::path& path) { OnTextureReimported(path); };
		m_InspectorWindow->OnOpenSpriteEditor = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_InspectorWindow->OnOpenAnimation = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_InspectorWindow->OnSelectEntity = [this](Entity entity) { m_Context.Selection = entity; };
		m_InspectorWindow->SetTerrainTool(&m_Context.Terrain);
		m_Context.Terrain.SetCommands(&m_Context.Commands);
		m_SpriteEditorWindow = CreateRef<SpriteEditorWindow>();
		m_SpriteEditorWindow->OnReimported = [this](const std::filesystem::path& path) { OnTextureReimported(path); };
		m_SpriteAnimationWindow = CreateRef<SpriteAnimationWindow>();
		m_SpriteAnimationWindow->OnSaved = [this](const std::filesystem::path& path) { OnAnimationSetSaved(path); };
		m_RenderSettingsWindow = CreateRef<RenderSettingsWindow>();
		m_RenderSettingsWindow->OnSaveProject = [this]() { SaveProject(); };
		m_ProjectSettingsWindow = CreateRef<ProjectSettingsWindow>();
		m_ProjectSettingsWindow->OnSaveProject = [this]() { SaveProject(); };
		m_BuildWindow = CreateRef<BuildWindow>();
		m_BuildWindow->OnBeforeBuild = [this]()
		{
			// A build packages the files on disk: save what is open first (the editor scene, never a play copy).
			SaveProject();
			if (m_Context.EditorScenePath.empty() == false)
			{
				SaveScene();
			}
		};
		m_EditorSettingsWindow = CreateRef<EditorSettingsWindow>();
		m_EditorSettingsWindow->OnSave = [this]() { SaveEditorSettings(); };
		m_EditorSettingsWindow->OnRevert = [this]() { RevertEditorSettings(); };

		m_ProjectHub = CreateRef<ProjectHubWindow>();
		m_ProjectHub->OnOpenProject = [this](const std::filesystem::path& path) { OpenProject(path); };
		m_ProjectHub->OnNewProject = [this](const std::string& name, const std::filesystem::path& location) { NewProject(name, location); };
		m_ProjectHub->OnBrowseProject = [this]() { OpenProject(); };
		m_ProjectHub->OnRecentChanged = [this]() { m_RecentProjects.Save(EditorPaths::GetRecentProjectsPath()); };
		m_RecentProjects.Load(EditorPaths::GetRecentProjectsPath());
		m_MenuBar.SetRecentProjects(&m_RecentProjects);
		m_MenuBar.OnOpenRecentProject = [this](const std::filesystem::path& path) { OpenProject(path); };
		m_MenuBar.OnCloseProject = [this]() { CloseProject(); };
		m_MenuBar.OnProjectHub = [this]() { m_ProjectHub->Open(); };

		m_MenuBar.OnNewProject = [this]() { m_ProjectHub->Open(true); };
		m_MenuBar.OnOpenProject = [this]() { OpenProject(); };
		m_MenuBar.OnSaveProject = [this]() { SaveProject(); };
		m_MenuBar.OnBuild = [this]() { m_BuildWindow->IsOpen() = true; };
		m_MenuBar.OnNewScene = [this]() { NewScene(); };
		m_MenuBar.OnNewEmptyScene = [this]() { NewEmptyScene(); };
		m_MenuBar.OnOpenScene = [this]() { OpenScene(); };
		m_MenuBar.OnSaveScene = [this]() { SaveScene(); };
		m_MenuBar.OnSaveSceneAs = [this]() { SaveSceneAs(); };
		m_MenuBar.OnPlay = [this]() { PlayScene(); };
		m_MenuBar.OnStop = [this]() { StopScene(); };
		m_Launch.OnOpenProject = [this](const std::filesystem::path& path) { OpenProject(path); };
		m_Launch.OnOpenScene = [this](const std::filesystem::path& path) { OpenScene(path); };
		m_Launch.OnPlay = [this]() { PlayScene(); };

		// The ImGui layer is attached before the client's layers, so its context exists and the editor's style
		// replaces the engine's default.
		LoadEditorSettings();
		RestoreOpenWindows();
		m_EditorViewport.GetCamera().SetProjectionType(m_Context.Settings.CameraStartsIn2D ? ProjectionType::Orthographic
			: ProjectionType::Perspective);

		CreateDefaultCamera();

		// After the demo scene: a project with a start scene replaces it.
		OpenStartupProject();
		m_Launch.Apply();
	}

	void EditorLayer::OnDetach()
	{
		// Before the windows go: their open state is part of the session saved below.
		StoreOpenWindows();

		// Released here, not in the destructor: Application::Shutdown detaches layers while the renderer is alive.
		m_ProjectHub = nullptr;
		m_SpriteAnimationWindow = nullptr;
		m_SpriteEditorWindow = nullptr;
		m_EditorSettingsWindow = nullptr;
		m_ProjectSettingsWindow = nullptr;
		m_BuildWindow = nullptr; // stops a build that is still running
		m_RenderSettingsWindow = nullptr;
		m_ProjectWindow = nullptr;
		m_InspectorWindow = nullptr;
		m_ConsoleWindow = nullptr;
		m_StatsWindow = nullptr;
		m_HierarchyWindow = nullptr;
		m_SceneRenderer.reset();
		m_GameScenes.Stop();
		m_Context.ActiveScene = nullptr;
		m_Context.EditorScene = nullptr;
		// The GPU is idle here (Application waits before detaching layers), so the views can go.
		m_GameViewport.Shutdown();
		m_EditorViewport.Shutdown();

		// Remember what was open for the next start (only the session part of the file changes).
		SaveSession();

		// The editor opened the project, so it closes it.
		AssetManager::OnProjectClosed();
		Project::Close();
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{
		const bool runGame = IsPlaying() && (m_Context.Paused == false || m_Context.StepFrames > 0);
		if (runGame)
		{
			// Scripts and engine systems, then the application's systems.
			m_Context.ActiveScene->OnUpdateRuntime(deltaTime);

			// A script may have asked for another scene (the scene was not touched while it ran).
			ProcessSceneTransition();

			if (m_Context.StepFrames > 0)
			{
				m_Context.StepFrames--;
			}
		}
		else
		{
			m_Context.ActiveScene->OnUpdateEditor(deltaTime);
		}

		UpdateAutosave(deltaTime);

		// One render per visible window; in game full screen the Editor window is covered.
		m_GameViewport.OnUpdate(deltaTime, *m_SceneRenderer);
		m_EditorViewport.OnUpdate(deltaTime, *m_SceneRenderer, m_GameViewport.IsFullscreen());
	}

	void EditorLayer::OnImGuiRender()
	{
		ImGuizmo::BeginFrame();
		m_GameViewport.UpdateKeys();
		CaptureControls::UpdateKeys(m_Context.Captures);
		m_MenuBar.UpdateKeys();
		m_Launch.Update();

		// No project (or File > Project Hub): the hub fills the window instead of the dockspace.
		const bool projectOpen = Project::GetActive() != nullptr;
		if (projectOpen == false || m_ProjectHub->IsOpen())
		{
			m_ProjectHub->OnImguiRender(m_RecentProjects, projectOpen);
			if (Project::GetActive() == nullptr || m_ProjectHub->IsOpen())
			{
				return;
			}
		}

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

		// The parent window isn't dockable: two docking targets inside each other would be confusing.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// While playing, every window drawn this frame takes on the play-mode tint (popped after End below).
		EditorChrome::PushPlayModeTint(m_Context.Settings.PlayMode, IsPlaying());

		// With PassthruCentralNode, DockSpace() draws the background and the pass-through hole, so Begin() doesn't.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		{
			window_flags |= ImGuiWindowFlags_NoBackground;
		}

		// Go on even when Begin() returns false (collapsed): an inactive DockSpace() would undock every window in it.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor dockspace", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
		{
			ImGui::PopStyleVar(2);
		}

		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		m_MenuBar.Draw(GetViewWindows());

		m_HierarchyWindow->OnImguiRender(m_Context.ActiveScene);
		// Picking an entity in the hierarchy takes the inspector back from an asset.
		const Entity selectedEntity = m_Context.Selection;
		if ((entt::entity)selectedEntity != m_LastSelectedEntity && (bool)selectedEntity)
		{
			m_Context.InspectedAsset.clear();
		}
		m_LastSelectedEntity = (entt::entity)selectedEntity;
		m_InspectorWindow->SetPlaying(IsPlaying());
		m_InspectorWindow->OnImguiRender(selectedEntity, m_Context.InspectedAsset);
		m_SpriteEditorWindow->OnImguiRender();
		m_SpriteAnimationWindow->OnImguiRender();
		m_StatsWindow->OnImguiRender(m_Context.ActiveScene);
		m_RenderSettingsWindow->OnImguiRender(m_Context.EditorScene.get(), m_Context.ActiveScene.get());
		m_ProjectSettingsWindow->OnImguiRender(m_Context.EditorScenePath);
		m_BuildWindow->OnImguiRender();
		m_EditorSettingsWindow->OnImguiRender(m_Context.Settings);

		m_ProjectWindow->OnImguiRender(m_Context.ActiveScene);
		// After the Project window, which it docks next to the first time.
		m_ConsoleWindow->OnImguiRender(m_Context.Settings.Console);

		m_EditorViewport.OnImGuiRender();
		m_GameViewport.OnImGuiRender();

		// After every window: inspector and gizmo edits of the selection, and render settings, become undo steps.
		m_EditWatcher.Update(m_Context);

		// A window opened or closed: remember it now, not only on close, so it holds when the debugger stops the editor.
		if (StoreOpenWindows())
		{
			SaveSession();
		}

		ImGui::End();

		// Over everything else, so it is drawn last.
		if (m_GameViewport.IsFullscreen())
		{
			m_GameViewport.DrawFullscreen();
		}

		EditorChrome::PopPlayModeTint();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		m_EditorViewport.OnEvent(e);
	}

	void EditorLayer::PlayScene()
	{
		if (IsPlaying())
		{
			return;
		}

		// A brush stroke in progress ends here (its undo step is kept); the tool waits for Stop.
		m_Context.Terrain.EndStroke(m_Context.Commands);

		const Entity selectedBefore = m_Context.Selection;

		// The game runs on a copy of the editor scene, at the bottom of the scene stack.
		Ref<Scene> runtimeScene = Scene::Copy(m_Context.EditorScene);
		runtimeScene->Init();
		m_GameScenes.Start(runtimeScene); // builds the physics world

		m_Context.ActiveScene = m_GameScenes.GetActiveScene();
		m_SceneRenderer->SetScene(m_Context.ActiveScene);
		m_EditorViewport.OnSceneChanged(); // a click on the old scene picks nothing in this one
		m_Context.Playing = true;
		m_Context.Paused = false;
		m_Context.StepFrames = 0;

		// The copy has the same UUIDs, so what was selected stays selected (the copy's entity).
		SelectSameEntity(selectedBefore, *m_Context.ActiveScene);
	}

	void EditorLayer::SelectSameEntity(Entity previous, Scene& scene)
	{
		if (previous.IsValid() && previous.HasComponent<IDComponent>())
		{
			m_Context.Selection = scene.GetEntityByUUID(previous.GetUUID());
		}
		else
		{
			m_Context.Selection = {};
		}
	}

	void EditorLayer::StopScene()
	{
		if (IsPlaying() == false)
		{
			return;
		}

		const Entity selectedBefore = m_Context.Selection;

		m_Context.Playing = false;
		m_Context.Paused = false;
		m_Context.StepFrames = 0;

		m_Context.ActiveScene = m_Context.EditorScene;
		m_SceneRenderer->SetScene(m_Context.ActiveScene);
		m_EditorViewport.OnSceneChanged(); // a click on the old scene picks nothing in this one

		// Back to the editor scene's entity with the same UUID; cleared if it only existed while playing
		// (made or destroyed by a script).
		SelectSameEntity(selectedBefore, *m_Context.ActiveScene);

		// Destroying the copy runs its scripts' OnDestroy; scenes pushed on top of it go with it, the top one first.
		m_GameScenes.Stop();

		if (m_Context.Settings.PlayMode.LeaveFullscreenOnStop)
		{
			m_GameViewport.SetFullscreen(false);
		}
	}

	void EditorLayer::ProcessSceneTransition()
	{
		// The scene stack does what a script asked for; the editor only follows the new scene.
		const SceneTransition::Type done = m_GameScenes.ProcessTransition(EditorPaths::GetScenesDirectory(), m_GameViewport.GetWidth(),
			m_GameViewport.GetHeight());
		if (done == SceneTransition::Type::None || IsPlaying() == false)
		{
			return;
		}

		if (done == SceneTransition::Type::Quit)
		{
			// A script quit the game: in the editor that stops playing, never closes Oxygen.
			HY_INFO("The game asked to quit: Play stops");
			StopScene();
			return;
		}

		m_Context.ActiveScene = m_GameScenes.GetActiveScene();
		m_SceneRenderer->SetScene(m_Context.ActiveScene);
		m_EditorViewport.OnSceneChanged(); // a click on the old scene picks nothing in this one
		m_Context.Selection = {};
	}

	void EditorLayer::NewProject(const std::string& name, const std::filesystem::path& location)
	{
		const std::filesystem::path directory = location / EditorPaths::ToFileName(name);
		const std::filesystem::path file = directory / (EditorPaths::ToFileName(name) + Project::FileExtension);
		std::error_code ec;
		if (std::filesystem::exists(file, ec))
		{
			HY_ERROR("New Project: {} already exists", file.string());
			return;
		}

		StopScene();
		Ref<Project> project = Project::New(file);
		project->GetConfig().Name = name;
		std::filesystem::create_directories(project->GetAssetDirectory() / "Scenes", ec);
		std::filesystem::create_directories(project->GetAssetDirectory() / "Prefabs", ec);
		// Caches, saves and builds stay out of the project's git.
		if (std::ofstream ignore(directory / ".gitignore"); ignore.is_open())
		{
			ignore << ".hydro/\nSaves/\nBuilds/\n";
		}
		AssetManager::OnProjectOpened(project->GetAssetDirectory());

		// A new project starts from the default render settings and belongs to no game until it uses a game type.
		Renderer::SetSettings(project->GetConfig().Renderer);
		SaveProject();
		m_Context.Settings.Session.LastProject = file;
		RememberProject();
		m_ProjectHub->Close();
		NewScene();
	}

	bool EditorLayer::OpenProject()
	{
		std::string filepath = FileDialogs::OpenFile("Hydro Project (*.hyproj)\0*.hyproj\0");

		if (filepath.empty())
		{
			return false;
		}

		OpenProject(filepath);

		return true;
	}

	void EditorLayer::OpenProject(const std::filesystem::path& path)
	{
		Ref<Project> project = Project::Load(path);
		if (project == nullptr)
		{
			return; // Already logged; the current project stays open.
		}

		std::error_code pathError;
		m_Context.Settings.Session.LastProject = std::filesystem::absolute(path, pathError);
		RememberProject();
		m_ProjectHub->Close();
		Renderer::SetSettings(project->GetConfig().Renderer);
		// Gives every asset file without a handle one (written into the file or a .hymeta next to it).
		AssetManager::OnProjectOpened(project->GetAssetDirectory());

		// An older project file names its start scene by path: turn that into a handle now.
		ProjectConfig& config = project->GetConfig();
		if ((uint64_t)config.StartScene == 0 && config.LegacyStartScene.empty() == false)
		{
			const AssetHandle handle = AssetManager::ImportAsset(config.LegacyStartScene);
			if ((uint64_t)handle != 0)
			{
				config.StartScene = handle;
				config.LegacyStartScene.clear();
				SaveProject();
				HY_INFO("Start scene {} is now stored by asset handle", AssetManager::GetMetadata(handle)->Path.generic_u8string());
			}
		}

		if ((uint64_t)config.StartScene == 0)
		{
			return;
		}

		const std::filesystem::path startScene = AssetManager::GetAbsolutePath(config.StartScene);
		std::error_code ec;
		if (AssetManager::IsAssetHandleValid(config.StartScene) && std::filesystem::exists(startScene, ec))
		{
			OpenScene(startScene);
			return;
		}

		// The start scene was deleted (#160): open the project's first scene instead of keeping the previous project's.
		// The project file is left as it is.
		const AssetHandle fallback = FindFirstScene();
		if ((uint64_t)fallback == 0)
		{
			HY_WARN("Project start scene (asset handle {}) is missing and the project has no other scene", (uint64_t)config.StartScene);
			return;
		}

		const std::filesystem::path fallbackScene = AssetManager::GetAbsolutePath(fallback);
		HY_WARN("Project start scene (asset handle {}) is missing; opening {} instead (set the start scene in Project Settings)",
			(uint64_t)config.StartScene, AssetManager::GetMetadata(fallback)->Path.generic_u8string());
		OpenScene(fallbackScene);
	}

	void EditorLayer::SaveProject()
	{
		Ref<Project> project = Project::GetActive();
		if (project == nullptr)
		{
			return;
		}

		project->GetConfig().Renderer = Renderer::GetSettings();
		Project::SaveActive();
	}

	void EditorLayer::LoadEditorSettings()
	{
		// No file yet (first run) or an unreadable one: the defaults (Hydro theme).
		EditorSettings settings;
		if (EditorSettingsSerializer::Load(EditorPaths::GetEditorSettingsPath(), settings) == false)
		{
			HY_INFO("No editor settings at {}; using the defaults", EditorPaths::GetEditorSettingsPath().string());
		}

		m_Context.Settings = settings;
		m_Context.Settings.Style.Apply();
		m_EditorViewport.GetCamera().SetSettings(m_Context.Settings.Camera);
	}

	void EditorLayer::OpenAsset(const std::filesystem::path& path)
	{
		if (Texture2D::IsSupportedFile(path))
		{
			m_SpriteEditorWindow->Open(path);
		}
		else if (path.extension() == SpriteAnimationSerializer::FileExtension)
		{
			m_SpriteAnimationWindow->Open(path);
		}
	}

	// The AssetManager reloads the texture into the same handle, the animation sets built on it are read again,
	// and sprites keep showing the same sprite by name.
	void EditorLayer::OnTextureReimported(const std::filesystem::path& imagePath)
	{
		const AssetHandle handle = AssetManager::FindHandle(imagePath);
		if (static_cast<uint64_t>(handle) == 0)
		{
			return;
		}

		// The sprites as they were, to find each one again by name after slicing moved them.
		const Ref<Texture2D> before = AssetManager::GetLoadedAsset<Texture2D>(handle);
		const Ref<Texture2D> texture = AssetManager::ReloadAsset<Texture2D>(handle);
		if (texture == nullptr)
		{
			return;
		}

		// A loaded animation set holds the texture it was built with, so each one on this image is read again.
		for (const auto& entry : AssetManager::GetRegistry().GetAll())
		{
			if (entry.second.Type != AssetType::SpriteAnimation)
			{
				continue;
			}

			const Ref<SpriteAnimationSet> set = AssetManager::GetLoadedAsset<SpriteAnimationSet>(entry.first);
			if (set != nullptr && set->GetTexture() != nullptr && set->GetTexture()->GetHandle() == handle)
			{
				AssetManager::ReloadAsset<SpriteAnimationSet>(entry.first);
			}
		}

		for (const Ref<Scene>& scene : { m_Context.EditorScene, m_GameScenes.GetActiveScene() })
		{
			if (scene == nullptr || before == nullptr || before == texture)
			{
				continue;
			}

			auto sprites = scene->m_Registry.view<SpriteRendererComponent>();
			for (auto entity : sprites)
			{
				SpriteRendererComponent& sprite = sprites.get<SpriteRendererComponent>(entity);
				if (sprite.Texture != handle)
				{
					continue;
				}

				// The same sprite by name: slicing again may have moved it.
				const int32_t index = texture->FindSprite(before->GetSprite(sprite.SpriteIndex).Name);
				sprite.SpriteIndex = index >= 0 ? (uint32_t)index : 0;
			}
		}
	}

	// Every animator on the set keeps its clip when it still exists (by name), and starts over otherwise.
	void EditorLayer::OnAnimationSetSaved(const std::filesystem::path& path)
	{
		const AssetHandle handle = AssetManager::FindHandle(path);
		if (static_cast<uint64_t>(handle) == 0)
		{
			return;
		}

		const Ref<SpriteAnimationSet> before = AssetManager::GetLoadedAsset<SpriteAnimationSet>(handle);
		const Ref<SpriteAnimationSet> set = AssetManager::ReloadAsset<SpriteAnimationSet>(handle);
		if (set == nullptr)
		{
			return;
		}

		for (const Ref<Scene>& scene : { m_Context.EditorScene, m_GameScenes.GetActiveScene() })
		{
			if (scene == nullptr)
			{
				continue;
			}

			auto animators = scene->m_Registry.view<SpriteAnimatorComponent>();
			for (auto entity : animators)
			{
				if (animators.get<SpriteAnimatorComponent>(entity).Animations != handle)
				{
					continue;
				}
				SpriteAnimator::RestartAfterReload(Entity(entity, scene.get()), before.get());
			}
		}
	}

	std::vector<EditorMenuBar::ViewWindow> EditorLayer::GetViewWindows()
	{
		return {
			{ "Console", &m_ConsoleWindow->IsOpen() },
			{ "Stats", &m_StatsWindow->IsOpen() },
			{ "Render Settings", &m_RenderSettingsWindow->IsOpen() },
			{ "Project Settings", &m_ProjectSettingsWindow->IsOpen() },
			{ "Editor Settings", &m_EditorSettingsWindow->IsOpen() },
			{ "Sprite Editor", &m_SpriteEditorWindow->IsOpen() },
			{ "Animation", &m_SpriteAnimationWindow->IsOpen() },
		};
	}

	void EditorLayer::RestoreOpenWindows()
	{
		const std::map<std::string, bool>& saved = m_Context.Settings.Session.OpenWindows;
		for (const EditorMenuBar::ViewWindow& window : GetViewWindows())
		{
			if (auto it = saved.find(window.Name); it != saved.end())
			{
				*window.Open = it->second;
			}
		}
	}

	bool EditorLayer::StoreOpenWindows()
	{
		bool changed = false;
		std::map<std::string, bool>& saved = m_Context.Settings.Session.OpenWindows;
		for (const EditorMenuBar::ViewWindow& window : GetViewWindows())
		{
			auto it = saved.find(window.Name);
			if (it == saved.end() || it->second != *window.Open)
			{
				saved[window.Name] = *window.Open;
				changed = true;
			}
		}
		return changed;
	}

	void EditorLayer::RevertEditorSettings()
	{
		const EditorSession session = m_Context.Settings.Session;
		LoadEditorSettings();
		m_Context.Settings.Session = session;
	}

	void EditorLayer::UpdateAutosave(float deltaTime)
	{
		const AutosaveSettings& autosave = m_Context.Settings.Autosave;
		if (autosave.Enabled == false || IsPlaying() || m_Context.EditorScenePath.empty())
		{
			m_AutosaveTimer = 0.0f;
			return;
		}

		m_AutosaveTimer += deltaTime;
		if (m_AutosaveTimer < autosave.IntervalMinutes * 60.0f)
		{
			return;
		}

		m_AutosaveTimer = 0.0f;
		SaveScene();
		HY_INFO("Autosaved {}", m_Context.EditorScenePath.filename().string());
	}

	void EditorLayer::OpenStartupProject()
	{
		const EditorStartupSettings& startup = m_Context.Settings.Startup;
		// Copy: opening a project or scene updates the session.
		const EditorSession session = m_Context.Settings.Session;

		std::error_code ec;
		if (startup.ReopenLastProject && session.LastProject.empty() == false && std::filesystem::exists(session.LastProject, ec))
		{
			OpenProject(session.LastProject);
		}

		// The project's own last scene; settings from older editors kept it in the user's file.
		std::filesystem::path lastScene = session.LastScene;
		EditorProjectState state;
		if (Ref<Project> project = Project::GetActive(); project != nullptr &&
			EditorSettingsSerializer::LoadProjectState(EditorPaths::GetProjectEditorStatePath(), state) && state.LastScene.empty() == false)
		{
			lastScene = project->GetProjectDirectory() / state.LastScene;
		}

		// The project opened its start scene; the last scene replaces it if it was another one.
		if (startup.ReopenLastScene && lastScene.empty() == false && lastScene != m_Context.EditorScenePath &&
			std::filesystem::exists(lastScene, ec))
		{
			OpenScene(lastScene);
		}
	}

	void EditorLayer::SaveSession()
	{
		EditorSettingsSerializer::SaveSession(m_Context.Settings.Session, EditorPaths::GetEditorSettingsPath());

		const Ref<Project> project = Project::GetActive();
		const std::filesystem::path& scene = m_Context.Settings.Session.LastScene;
		if (project == nullptr || scene.empty())
		{
			return;
		}

		// Only a scene inside this project is remembered for it.
		std::error_code ec;
		const std::filesystem::path relative = std::filesystem::relative(scene, project->GetProjectDirectory(), ec);
		if (ec || relative.empty() || *relative.begin() == "..")
		{
			return;
		}

		EditorProjectState state;
		state.LastScene = relative;
		EditorSettingsSerializer::SaveProjectState(state, EditorPaths::GetProjectEditorStatePath());
	}

	void EditorLayer::SaveEditorSettings()
	{
		EditorSettingsSerializer::Save(m_Context.Settings, EditorPaths::GetEditorSettingsPath());
	}

	void EditorLayer::CloseProject()
	{
		if (Project::GetActive() == nullptr)
		{
			return;
		}

		StopScene();
		SaveSession();
		AssetManager::OnProjectClosed();
		Project::Close();
		// Closed on purpose: the next start shows the hub instead of reopening it.
		m_Context.Settings.Session.LastProject.clear();
		m_Context.Settings.Session.LastScene.clear();
		SaveSession();
		NewEmptyScene();
		HY_INFO("Project closed");
	}

	void EditorLayer::RememberProject()
	{
		const Ref<Project> project = Project::GetActive();
		if (project == nullptr)
		{
			return;
		}

		RecentProject entry;
		std::error_code ec;
		entry.Path = std::filesystem::absolute(project->GetProjectFilePath(), ec).lexically_normal();
		entry.Name = project->GetConfig().Name;
		entry.Game = project->GetConfig().Game;
		entry.Editor = Process::GetExecutablePath();
		m_RecentProjects.Add(entry);
		m_RecentProjects.Save(EditorPaths::GetRecentProjectsPath());
	}

	void EditorLayer::SetEditorScene(const Ref<Scene>& scene)
	{
		StopScene();
		// Undo steps belong to the scene they were made in.
		m_Context.Terrain.EndStroke(m_Context.Commands);
		m_Context.Commands.Clear();

		m_Context.EditorScene = scene;
		m_Context.EditorScene->Init();
		m_Context.EditorScene->OnViewportResize(m_GameViewport.GetWidth(), m_GameViewport.GetHeight());
		m_Context.ActiveScene = m_Context.EditorScene;
		m_SceneRenderer->SetScene(m_Context.ActiveScene);
		m_EditorViewport.OnSceneChanged(); // a click on the old scene picks nothing in this one
		m_Context.Selection = {};
	}

	void EditorLayer::NewScene()
	{
		// The project's scene template, when it has one that loads: the new scene starts as a copy of it.
		Ref<Project> project = Project::GetActive();
		const AssetHandle sceneTemplate = project != nullptr ? project->GetConfig().SceneTemplate : AssetHandle(0);
		if ((uint64_t)sceneTemplate != 0)
		{
			const AssetMetadata* metadata = AssetManager::GetMetadata(sceneTemplate);
			if (metadata != nullptr && metadata->IsMissing == false)
			{
				Ref<Scene> scene = CreateRef<Scene>("Untitled", true);
				SceneSerializer serializer(scene);
				if (serializer.Deserialize(AssetManager::GetAssetDirectory() / metadata->Path))
				{
					scene->SetName("Untitled");
					SetEditorScene(scene);
					m_Context.EditorScenePath.clear();
					m_Context.Settings.Session.LastScene.clear();
					HY_INFO("New scene from the template {}", metadata->Path.generic_string());
					return;
				}
			}
			HY_WARN("New Scene: the project's scene template can't be loaded; starting an empty scene");
		}
		NewEmptyScene();
	}

	void EditorLayer::NewEmptyScene()
	{
		SetEditorScene(CreateRef<Scene>("Untitled", true));
		CreateDefaultCamera();
		m_Context.EditorScenePath.clear();
		m_Context.Settings.Session.LastScene.clear();
	}

	void EditorLayer::CreateDefaultCamera()
	{
		Entity camera = m_Context.EditorScene->CreateEntity("Camera");
		camera.AddComponent<TransformComponent>();
		camera.AddComponent<CameraComponent>();
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Hydro Scene (*.hyscene)\0*.hyscene\0");

		if (filepath.empty())
		{
			return;
		}

		OpenScene(filepath);
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path)
	{
		if (path.extension().string() != SceneSerializer::FileExtension)
		{
			HY_WARN("Could not load {} - not a scene file", path.filename().string());
			return;
		}

		// Load into a fresh scene and only swap it in when loading succeeded.
		Ref<Scene> newScene = CreateRef<Scene>("Untitled", true);
		SceneSerializer serializer(newScene);
		if (serializer.Deserialize(path) == false)
		{
			return;
		}

		// The file name is the scene's name, so a scene renamed on disk shows its new name.
		newScene->SetName(path.stem().string());
		SetEditorScene(newScene);
		m_Context.EditorScenePath = path;

		std::error_code ec;
		m_Context.Settings.Session.LastScene = std::filesystem::absolute(path, ec);
	}

	void EditorLayer::SaveScene()
	{
		// A new scene's first save goes to Assets/Scenes/<SceneName>.hyscene, numbered ("Untitled 2") so it never
		// overwrites another scene.
		if (m_Context.EditorScenePath.empty())
		{
			const std::filesystem::path directory = EditorPaths::GetScenesDirectory();
			const std::string baseName = EditorPaths::ToFileName(m_Context.EditorScene->GetName());
			std::string name = baseName;
			std::error_code ec;
			for (int i = 2; std::filesystem::exists(directory / (name + SceneSerializer::FileExtension), ec); i++)
			{
				name = baseName + " " + std::to_string(i);
			}

			std::filesystem::create_directories(directory, ec);
			m_Context.EditorScene->SetName(name);
			m_Context.EditorScenePath = directory / (name + SceneSerializer::FileExtension);
		}

		// Terrain assets edited in this scene are written with it (a stroke in progress ends first).
		m_Context.Terrain.EndStroke(m_Context.Commands);
		TerrainTool::SaveModifiedTerrains(*m_Context.EditorScene);

		// Always the editor scene: what happens while playing is not saved.
		SceneSerializer serializer(m_Context.EditorScene);
		if (serializer.Serialize(m_Context.EditorScenePath))
		{
			std::error_code ec;
			m_Context.Settings.Session.LastScene = std::filesystem::absolute(m_Context.EditorScenePath, ec);
			m_Context.Commands.MarkSaved();
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("Hydro Scene (*.hyscene)\0*.hyscene\0");

		if (filepath.empty())
		{
			return;
		}

		std::filesystem::path path = filepath;
		if (path.extension().string() != SceneSerializer::FileExtension)
		{
			path += SceneSerializer::FileExtension;
		}

		// Name the scene after its file.
		m_Context.EditorScene->SetName(path.stem().string());
		m_Context.Terrain.EndStroke(m_Context.Commands);
		TerrainTool::SaveModifiedTerrains(*m_Context.EditorScene);

		SceneSerializer serializer(m_Context.EditorScene);
		if (serializer.Serialize(path))
		{
			m_Context.EditorScenePath = path;

			std::error_code ec;
			m_Context.Settings.Session.LastScene = std::filesystem::absolute(path, ec);
			m_Context.Commands.MarkSaved();
		}
	}

	void EditorLayer::OnPathRenamed(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		// The open and last scenes are remembered by path, so they follow a rename here. Assets referenced by handle
		// follow on their own.
		if (PathFollower::Follow(m_Context.EditorScenePath, from, to))
		{
			m_Context.EditorScene->SetName(m_Context.EditorScenePath.stem().string());
		}
		PathFollower::Follow(m_Context.Settings.Session.LastScene, from, to);
		PathFollower::Follow(m_Context.InspectedAsset, from, to);
		m_SpriteEditorWindow->OnAssetMoved(from, to);
		m_SpriteAnimationWindow->OnAssetMoved(from, to);
	}

	void EditorLayer::OnPathDeleted(const std::filesystem::path& path)
	{
		// What was open for editing is gone; saving it would bring the file back.
		if (PathFollower::Follow(m_Context.InspectedAsset, path, path))
		{
			m_Context.InspectedAsset.clear();
		}
		m_SpriteEditorWindow->OnAssetDeleted(path);
		m_SpriteAnimationWindow->OnAssetDeleted(path);
	}

	void EditorLayer::OnAssetDropped(const std::filesystem::path& path)
	{
		const std::string extension = path.extension().string();

		if (extension == PrefabSerializer::FileExtension)
		{
			InstantiatePrefab(path);
		}
		else if (extension == SceneSerializer::FileExtension)
		{
			OpenScene(path);
		}
		else if (GetAssetTypeFromExtension(path) == AssetType::Mesh)
		{
			// A model: an entity with a Mesh component at the camera's focus, on the ground, using the import's materials.
			const AssetHandle handle = AssetManager::FindHandle(path);
			const Ref<Mesh> model = (uint64_t)handle != 0 ? AssetManager::GetAsset<Mesh>(handle) : nullptr;
			if (model == nullptr)
			{
				return;
			}

			SceneEditScope undo = m_Context.BeginSceneEdit("Place " + path.stem().u8string());
			Entity entity = m_Context.ActiveScene->CreateEntity(path.stem().u8string());
			const glm::vec3 focus = m_EditorViewport.GetCamera().GetFocalPoint();
			entity.AddComponent<TransformComponent>().Translation = { focus.x, focus.y, 0.0f };
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.Model = handle;
			mesh.Materials.clear();
			m_Context.Selection = entity;
			m_Context.FinishSceneEdit(undo);
		}
	}

	Entity EditorLayer::InstantiatePrefab(const std::filesystem::path& path)
	{
		if (m_Context.ActiveScene == nullptr)
		{
			return {};
		}
		SceneEditScope undo = m_Context.BeginSceneEdit("Instantiate " + path.stem().u8string());
		Entity entity = PrefabSerializer::Instantiate(*m_Context.ActiveScene, path);
		if (entity)
		{
			m_Context.Selection = entity;
		}
		m_Context.FinishSceneEdit(undo);
		return entity;
	}
}
