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
		m_ProjectWindow->OnEntityCreated = [this](Entity entity) { m_Context.Selection = entity; };
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

		// The ImGui layer is attached before the client's layers, so its context exists and the editor's style
		// replaces the engine's default.
		LoadEditorSettings();
		RestoreOpenWindows();
		m_EditorViewport.GetCamera().SetProjectionType(m_Context.Settings.CameraStartsIn2D ? ProjectionType::Orthographic
			: ProjectionType::Perspective);

		CreateDefaultCamera();

		// After the demo scene: a project with a start scene replaces it.
		OpenStartupProject();
		ApplyLaunchOptions();
	}

	EditorLaunchOptions EditorLaunchOptions::Parse(const ApplicationCommandLine& commandLine)
	{
		EditorLaunchOptions options;
		options.Project = commandLine.Get("--project");
		options.Scene = commandLine.Get("--scene");
		options.Play = commandLine.Has("--play");
		const std::string frames = commandLine.Get("--frames");
		if (frames.empty() == false)
		{
			options.Frames = (uint32_t)std::max(0, std::atoi(frames.c_str()));
		}
		options.Capture = commandLine.Get("--capture");
		options.Validate = commandLine.Has("--validate");
		options.Exit = commandLine.Has("--exit") || options.Capture.empty() == false || options.Validate;
		return options;
	}

	void EditorLayer::ApplyLaunchOptions()
	{
		if (m_Launch.Project.empty() == false)
		{
			std::error_code ec;
			if (std::filesystem::exists(m_Launch.Project, ec))
			{
				OpenProject(m_Launch.Project);
			}
			else
			{
				HY_ERROR("--project: {} doesn't exist", m_Launch.Project.string());
			}
		}
		if (m_Launch.Scene.empty() == false)
		{
			const std::filesystem::path scene = FindScene(m_Launch.Scene);
			if (scene.empty())
			{
				HY_ERROR("--scene: no scene '{}' in the project", m_Launch.Scene);
			}
			else
			{
				OpenScene(scene);
			}
		}

		if (m_Launch.Validate)
		{
			const bool clean = ValidateProject();
			Application::Get().Quit(clean ? 0 : 1);
			return;
		}
		if (m_Launch.Play)
		{
			PlayScene();
		}
		if (m_Launch.IsAutomated())
		{
			// The Game view is drawn whatever the docking says, and at the capture size.
			if (m_Launch.Capture.empty() == false)
			{
				m_GameViewport.SetFullscreen(true);
				m_Context.Captures.FixedSize = true;
			}
			m_LaunchStep = LaunchStep::Waiting;
			m_LaunchFramesLeft = m_Launch.Frames;
			HY_INFO("Unattended run: {} frames{}{}", m_Launch.Frames,
				m_Launch.Capture.empty() ? "" : ", then capture " + m_Launch.Capture.string(),
				m_Launch.Exit ? ", then exit" : "");
		}
	}

	std::filesystem::path EditorLayer::FindScene(const std::string& nameOrPath) const
	{
		std::error_code ec;
		const std::filesystem::path given = nameOrPath;
		if (given.extension().string() == SceneSerializer::FileExtension && std::filesystem::exists(given, ec))
		{
			return given;
		}
		if (Project::GetActive() == nullptr)
		{
			return {};
		}
		const std::filesystem::path inProject = EditorPaths::GetScenesDirectory() / (given.stem().string() + SceneSerializer::FileExtension);
		if (std::filesystem::exists(inProject, ec))
		{
			return inProject;
		}
		return {};
	}

	bool EditorLayer::ValidateProject()
	{
		if (Project::GetActive() == nullptr)
		{
			HY_ERROR("--validate: no project is open");
			return false;
		}

		std::vector<LogMessage> messages;
		const uint64_t before = Log::GetMessagesSince(0, messages);
		uint32_t scenes = 0;
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(Project::GetActive()->GetAssetDirectory(), ec))
		{
			if (entry.is_regular_file() == false || entry.path().extension().string() != SceneSerializer::FileExtension)
			{
				continue;
			}
			// Into a scratch scene, like OpenScene; the serializer logs every missing asset and script.
			Ref<Scene> scene = CreateRef<Scene>("Validate", true);
			SceneSerializer serializer(scene);
			if (serializer.Deserialize(entry.path()))
			{
				HY_INFO("Validated {}: {} entities", entry.path().filename().string(), scene->m_Registry.size<IDComponent>());
			}
			else
			{
				HY_ERROR("Validate: {} failed to load", entry.path().filename().string());
			}
			scenes++;
		}

		messages.clear();
		Log::GetMessagesSince(before, messages);
		uint32_t problems = 0;
		for (const LogMessage& message : messages)
		{
			if (message.Level >= LogLevel::Warn)
			{
				problems++;
			}
		}
		HY_INFO("Validated {} scenes: {} warnings or errors", scenes, problems);
		return problems == 0;
	}

	void EditorLayer::UpdateLaunchAutomation()
	{
		switch (m_LaunchStep)
		{
			case LaunchStep::Waiting:
			{
				if (m_LaunchFramesLeft > 0)
				{
					m_LaunchFramesLeft--;
					return;
				}
				if (m_Launch.Capture.empty())
				{
					m_LaunchStep = LaunchStep::Done;
					return;
				}
				// Through the normal capture request, so the view is drawn at the capture size first.
				std::error_code ec;
				std::filesystem::create_directories(m_Launch.Capture.parent_path(), ec);
				m_Context.Captures.Game = true;
				m_Context.Captures.GamePath = m_Launch.Capture;
				m_LaunchStep = LaunchStep::Capturing;
				return;
			}
			case LaunchStep::Capturing:
			{
				// The request is taken when the Game view is drawn, and the file is written after that frame.
				if (m_Context.Captures.Game == false && m_GameViewport.IsCapturePending() == false)
				{
					HY_INFO("Captured the Game view to {}", m_Launch.Capture.string());
					m_LaunchStep = LaunchStep::Done;
				}
				return;
			}
			case LaunchStep::Done:
			{
				if (m_Launch.Exit)
				{
					Application::Get().Quit(0);
				}
				m_LaunchStep = LaunchStep::None;
				return;
			}
			case LaunchStep::None:
			{
				return;
			}
		}
	}

	void EditorLayer::OnDetach()
	{
		// Before the windows go: their open state is part of the session saved below.
		StoreOpenWindows();

		// Released here, not in the destructor: Application::Shutdown detaches layers while the renderer is alive.
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
		EditorSettingsSerializer::SaveSession(m_Context.Settings.Session, EditorPaths::GetEditorSettingsPath());

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
		UpdateUndoKeys();
		UpdateLaunchAutomation();

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

		if (ImGui::BeginMenuBar())
		{
			EditorChrome::DrawBrand(m_Context.Settings.Style.GetHighlightColor());

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Project..."))
				{
					NewProject();
				}
				if (ImGui::MenuItem("Open Project..."))
				{
					OpenProject();
				}
				if (ImGui::MenuItem("Save Project", nullptr, false, Project::GetActive() != nullptr))
				{
					SaveProject();
				}
				if (ImGui::MenuItem("Build...", nullptr, false, Project::GetActive() != nullptr))
				{
					m_BuildWindow->IsOpen() = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					NewScene();
				}
				if (ImGui::MenuItem("New Empty Scene"))
				{
					NewEmptyScene();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("A scene with only a camera, without the project's scene template");
				}
				if (ImGui::MenuItem("Open Scene..."))
				{
					OpenScene();
				}
				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
				{
					SaveScene();
				}
				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
				{
					SaveSceneAs();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit"))
				{
					Application::Get().Quit();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				const std::string undoName = m_Context.Commands.GetUndoName();
				const std::string redoName = m_Context.Commands.GetRedoName();
				const std::string undoLabel = undoName.empty() ? std::string("Undo") : "Undo " + undoName;
				const std::string redoLabel = redoName.empty() ? std::string("Redo") : "Redo " + redoName;
				if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, m_Context.Commands.CanUndo() && IsPlaying() == false))
				{
					Undo();
				}
				if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, m_Context.Commands.CanRedo() && IsPlaying() == false))
				{
					Redo();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				for (const ViewWindow& window : GetViewWindows())
				{
					ImGui::MenuItem(window.Name, nullptr, window.Open);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Empty Entity"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Entity");
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Sprite"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Sprite");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<SpriteRendererComponent>();
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Camera"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Camera");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<CameraComponent>();
					m_Context.Selection = entity;
				}
				if (ImGui::MenuItem("Light"))
				{
					Entity entity = m_Context.ActiveScene->CreateEntity("Light");
					entity.AddComponent<TransformComponent>();
					m_Context.Selection = entity;
				}
				if (ImGui::BeginMenu("Meshes"))
				{
					if (ImGui::MenuItem("Box"))
					{
						Entity entity = m_Context.ActiveScene->CreateEntity("Box");
						entity.AddComponent<TransformComponent>();
						entity.AddComponent<MeshComponent>().Shape = MeshShape::Box;
						m_Context.Selection = entity;
					}
					if (ImGui::MenuItem("Cylinder"))
					{
						Entity entity = m_Context.ActiveScene->CreateEntity("Cylinder");
						entity.AddComponent<TransformComponent>();
						entity.AddComponent<MeshComponent>().Shape = MeshShape::Cylinder;
						m_Context.Selection = entity;
					}
					if (ImGui::MenuItem("Plane"))
					{
						Entity entity = m_Context.ActiveScene->CreateEntity("Plane");
						entity.AddComponent<TransformComponent>();
						entity.AddComponent<MeshComponent>().Shape = MeshShape::Plane;
						m_Context.Selection = entity;
					}
					if (ImGui::MenuItem("Stairs"))
					{
						Entity entity = m_Context.ActiveScene->CreateEntity("Stairs");
						entity.AddComponent<TransformComponent>();
						entity.AddComponent<MeshComponent>().Shape = MeshShape::Stairs;
						m_Context.Selection = entity;
					}
					if (ImGui::MenuItem("Ramp"))
					{
						Entity entity = m_Context.ActiveScene->CreateEntity("Ramp");
						entity.AddComponent<TransformComponent>();
						entity.AddComponent<MeshComponent>().Shape = MeshShape::Ramp;
						m_Context.Selection = entity;
					}


					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}


			DrawPlayControls();

			// The waterline: the accent while editing, the play-mode tint while playing.
			const bool tinted = IsPlaying() && m_Context.Settings.PlayMode.Tint;
			EditorChrome::DrawMenuBarWaterline(tinted ? m_Context.Settings.PlayMode.TintColor
				: m_Context.Settings.Style.GetHighlightColor());

			ImGui::EndMenuBar();
		}

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

		// A window opened or closed: remember it now, not only on close, so it holds when the debugger stops the editor.
		if (StoreOpenWindows())
		{
			EditorSettingsSerializer::SaveSession(m_Context.Settings.Session, EditorPaths::GetEditorSettingsPath());
		}

		ImGui::End();

		// Over everything else, so it is drawn last.
		if (m_GameViewport.IsFullscreen())
		{
			m_GameViewport.DrawFullscreen();
		}

		EditorChrome::PopPlayModeTint();
	}

	void EditorLayer::DrawPlayControls()
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonSize = ImGui::GetFrameHeight() * 1.4f;
		const float totalWidth = 3.0f * buttonSize + 2.0f * style.ItemSpacing.x;
		const float centre = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
		if (centre > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(centre);
		}

		const ImVec4 accent = m_Context.Settings.Style.GetHighlightColor();
		const ImVec4 playColor = m_Context.Settings.PlayMode.Tint ? m_Context.Settings.PlayMode.TintColor : accent;

		// Play turns into Stop (in the play-mode colour) while the game runs.
		const bool playing = IsPlaying();
		if (EditorChrome::IconButton("##PlayStop", playing ? ToolbarIcon::Stop : ToolbarIcon::Play, buttonSize, playing, playColor,
			playing ? "Stop (back to the editor scene)" : "Play (runs a copy of the scene)"))
		{
			if (playing)
			{
				StopScene();
			}
			else
			{
				PlayScene();
			}
		}

		ImGui::BeginDisabled(playing == false);

		const bool paused = m_Context.Paused;
		if (EditorChrome::IconButton("##Pause", paused ? ToolbarIcon::Play : ToolbarIcon::Pause, buttonSize, paused, accent,
			paused ? "Resume" : "Pause"))
		{
			m_Context.Paused = m_Context.Paused == false;
		}

		ImGui::BeginDisabled(m_Context.Paused == false);
		if (EditorChrome::IconButton("##Step", ToolbarIcon::Step, buttonSize, false, accent, "Step one frame"))
		{
			m_Context.StepFrames++;
		}
		ImGui::EndDisabled();

		ImGui::EndDisabled();
	}

	void EditorLayer::UpdateUndoKeys()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (IsPlaying() || io.WantTextInput || io.KeyCtrl == false)
		{
			return;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			if (io.KeyShift)
			{
				Redo();
			}
			else
			{
				Undo();
			}
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
		{
			Redo();
		}
	}

	void EditorLayer::Undo()
	{
		// A stroke in progress becomes a step first, so it is what is undone.
		m_Context.Terrain.EndStroke(m_Context.Commands);
		m_Context.Commands.Undo();
	}

	void EditorLayer::Redo()
	{
		m_Context.Terrain.EndStroke(m_Context.Commands);
		m_Context.Commands.Redo();
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

	void EditorLayer::NewProject()
	{
		std::string directory = FileDialogs::OpenDirectory();

		if (directory.empty())
		{
			return;
		}

		// <directory>/<directory name>.hyproj, with the standard asset folders.
		const std::filesystem::path projectDirectory = directory;
		const std::string name = EditorPaths::ToFileName(projectDirectory.filename().string());
		Ref<Project> project = Project::New(projectDirectory / (name + Project::FileExtension));
		m_Context.Settings.Session.LastProject = projectDirectory / (name + Project::FileExtension);

		std::error_code ec;
		std::filesystem::create_directories(project->GetAssetDirectory() / "Scenes", ec);
		std::filesystem::create_directories(project->GetAssetDirectory() / "Prefabs", ec);
		AssetManager::OnProjectOpened(project->GetAssetDirectory());

		// A new project starts from the default render settings.
		Renderer::SetSettings(project->GetConfig().Renderer);
		SaveProject();
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

	std::vector<EditorLayer::ViewWindow> EditorLayer::GetViewWindows()
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
		for (const ViewWindow& window : GetViewWindows())
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
		for (const ViewWindow& window : GetViewWindows())
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

		if (Project::GetActive() == nullptr)
		{
			OpenDefaultProject();
		}

		// The project opened its start scene; the last scene replaces it if it was another one.
		if (startup.ReopenLastScene && session.LastScene.empty() == false && session.LastScene != m_Context.EditorScenePath &&
			std::filesystem::exists(session.LastScene, ec))
		{
			OpenScene(session.LastScene);
		}
	}

	void EditorLayer::SaveEditorSettings()
	{
		EditorSettingsSerializer::Save(m_Context.Settings, EditorPaths::GetEditorSettingsPath());
	}

	void EditorLayer::OpenDefaultProject()
	{
		const std::filesystem::path directory = EditorPaths::GetDefaultProjectDirectory();

		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
		{
			if (entry.is_regular_file() && entry.path().extension().string() == Project::FileExtension)
			{
				OpenProject(entry.path());
				if (Project::GetActive() != nullptr)
				{
					return;
				}
			}
		}

		// None found (or it failed to load): an unsaved project in the default folder, so the editor always has one.
		Ref<Project> project = Project::New(directory / "Sandbox.hyproj");
		AssetManager::OnProjectOpened(project->GetAssetDirectory());
		HY_INFO("No project found in {}; started an unsaved one", directory.string());
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
			if (Entity entity = PrefabSerializer::Instantiate(*m_Context.ActiveScene, path))
			{
				m_Context.Selection = entity;
			}
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

			Entity entity = m_Context.ActiveScene->CreateEntity(path.stem().u8string());
			const glm::vec3 focus = m_EditorViewport.GetCamera().GetFocalPoint();
			entity.AddComponent<TransformComponent>().Translation = { focus.x, focus.y, 0.0f };
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.Model = handle;
			mesh.Materials.clear();
			m_Context.Selection = entity;
		}
	}
}
