#include "EditorLayer.h"
#include <iostream>

#include <imgui/imgui.h>
#include <string>

#include "Hydro/Utils/PlatformUtils.h"
#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Scene/SpriteAnimation.h"
#include "EditorPaths.h"
#include "Utilities/AssetDragDrop.h"
#include "Utilities/EditorChrome.h"
#include "Utilities/PathFollower.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <random>

namespace Hydro
{
	static glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

	void EditorLayer::OnAttach()
	{
		m_EditorScene = CreateRef<Scene>("Scene", false);
		m_EditorScene->Init();
		m_ActiveScene = m_EditorScene;
		m_SceneRenderer = CreateScope<SceneRenderer>(m_ActiveScene);

		RenderViewSpecification editorViewSpecification;
		editorViewSpecification.DebugName = "Editor";
		editorViewSpecification.EditorView = true;
		m_EditorView = CreateScope<RenderView>(editorViewSpecification);

		RenderViewSpecification gameViewSpecification;
		gameViewSpecification.DebugName = "Game";
		m_GameView = CreateScope<RenderView>(gameViewSpecification);

		m_HierarchyWindow = CreateRef<HierachyWindow>();
		m_StatsWindow = CreateRef<StatsWindow>();
		m_ConsoleWindow = CreateRef<ConsoleWindow>();
		m_InspectorWindow = CreateRef<InspectorWindow>();
		m_ProjectWindow = CreateRef<ProjectWindow>();
		m_ProjectWindow->OnOpenScene = [this](const std::filesystem::path& path) { OpenScene(path); };
		m_ProjectWindow->OnEntityCreated = [this](Entity entity) { m_HierarchyWindow->SetSelectedEntity(entity); };
		m_HierarchyWindow->OnEntityFocus = [this](Entity entity) { FocusEntity(entity); };
		m_ProjectWindow->OnPathRenamed = [this](const std::filesystem::path& from, const std::filesystem::path& to) { OnPathRenamed(from, to); };
		m_ProjectWindow->OnPathDeleted = [this](const std::filesystem::path& path) { OnPathDeleted(path); };
		m_ProjectWindow->OnAssetSelected = [this](const std::filesystem::path& path)
		{
			// The inspector shows the asset instead of an entity.
			m_InspectedAsset = path;
			m_HierarchyWindow->ClearSelection();
		};
		m_ProjectWindow->OnOpenAsset = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_InspectorWindow->OnTextureReimported = [this](const std::filesystem::path& path) { OnTextureReimported(path); };
		m_InspectorWindow->OnOpenSpriteEditor = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_InspectorWindow->OnOpenAnimation = [this](const std::filesystem::path& path) { OpenAsset(path); };
		m_SpriteEditorWindow = CreateRef<SpriteEditorWindow>();
		m_SpriteEditorWindow->OnReimported = [this](const std::filesystem::path& path) { OnTextureReimported(path); };
		m_SpriteAnimationWindow = CreateRef<SpriteAnimationWindow>();
		m_SpriteAnimationWindow->OnSaved = [this](const std::filesystem::path& path) { OnAnimationSetSaved(path); };
		m_RenderSettingsWindow = CreateRef<RenderSettingsWindow>();
		m_RenderSettingsWindow->OnSaveProject = [this]() { SaveProject(); };
		m_ProjectSettingsWindow = CreateRef<ProjectSettingsWindow>();
		m_ProjectSettingsWindow->OnSaveProject = [this]() { SaveProject(); };
		m_EditorSettingsWindow = CreateRef<EditorSettingsWindow>();
		m_EditorSettingsWindow->OnSave = [this]() { SaveEditorSettings(); };
		m_EditorSettingsWindow->OnRevert = [this]() { RevertEditorSettings(); };

		// The ImGui layer is attached before the client's layers, so its context exists here and the
		// editor's style replaces the engine's default one.
		LoadEditorSettings();
		RestoreOpenWindows();
		m_EditorCamera.SetProjectionType(m_EditorSettings.CameraStartsIn2D ? ProjectionType::Orthographic : ProjectionType::Perspective);

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
				SetGameFullscreen(true);
				m_CaptureFixedSize = true;
			}
			m_LaunchStep = LaunchStep::Waiting;
			m_LaunchFramesLeft = m_Launch.Frames;
			HY_INFO("Unattended run: {} frames{}{}", m_Launch.Frames, m_Launch.Capture.empty() ? "" : ", then capture " + m_Launch.Capture.string(),
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
				m_CaptureGameRequested = true;
				m_LaunchStep = LaunchStep::Capturing;
				return;
			}
			case LaunchStep::Capturing:
			{
				// The request is taken when the Game view is drawn, and the file is written after that frame.
				if (m_CaptureGameRequested == false && m_GameView->IsCapturePending() == false)
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

		// Release everything OnAttach created, here rather than in the destructor:
		// Application::Shutdown detaches layers while the renderer is still alive.
		m_SpriteAnimationWindow = nullptr;
		m_SpriteEditorWindow = nullptr;
		m_EditorSettingsWindow = nullptr;
		m_ProjectSettingsWindow = nullptr;
		m_RenderSettingsWindow = nullptr;
		m_ProjectWindow = nullptr;
		m_InspectorWindow = nullptr;
		m_ConsoleWindow = nullptr;
		m_StatsWindow = nullptr;
		m_HierarchyWindow = nullptr;
		m_SceneRenderer.reset();
		m_SuspendedScenes.clear();
		m_RuntimeScene = nullptr;
		m_ActiveScene = nullptr;
		m_EditorScene = nullptr;
		// The GPU is idle here (Application waits before detaching layers), so the views can go.
		m_GameView.reset();
		m_EditorView.reset();

		// Remember what was open for the next start (only the session part of the file changes).
		EditorSettingsSerializer::SaveSession(m_EditorSettings.Session, EditorPaths::GetEditorSettingsPath());

		// The editor opened the project, so it closes it.
		AssetManager::OnProjectClosed();
		Project::Close();
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{
		// deltaTime comes from the Time subsystem (Time::GetDeltaTime()), passed in by Application::Run.
		// 1-3. The scene runs only in play mode (and not while paused, except for a Step).
		// The editor's window goes in or out of full screen here, between ImGui frames (SetGameFullscreen asks).
		if (m_PendingWindowFullscreen >= 0)
		{
			Window& window = Application::Get().GetWindow();
			window.SetFullscreen(m_PendingWindowFullscreen == 1);
			m_WindowFullscreenByGame = m_PendingWindowFullscreen == 1 && window.IsFullscreen();
			m_PendingWindowFullscreen = -1;
		}
		m_FullscreenHintTime = std::max(0.0f, m_FullscreenHintTime - deltaTime);

		const bool runGame = IsPlaying() && (m_Paused == false || m_StepFrames > 0);
		if (runGame)
		{
			// Scripts and engine systems, then the application's systems.
			m_ActiveScene->OnUpdateRuntime(deltaTime);

			// A script may have asked for another scene (the scene was not touched while it ran).
			ProcessSceneTransition();

			if (m_StepFrames > 0)
				m_StepFrames--;
		}
		else
		{
			m_ActiveScene->OnUpdateEditor(deltaTime);
		}

		UpdateAutosave(deltaTime);

		// 4. Render submission: one view per visible window. The editor camera is updated every frame
		// so a drag never jumps, but only takes new input over the Editor window. Its settings may
		// have changed in the Editor Settings window (SetSettings does nothing when they didn't).
		m_EditorCamera.SetSettings(m_EditorSettings.Camera);
		m_EditorCamera.Update(deltaTime, m_EditorViewVisible && m_EditorViewHovered);

		// In game full screen the Editor window is covered: don't draw it.
		if (m_EditorViewVisible && m_GameFullscreen == false)
		{
			SceneRenderer::EditorOverlay overlay;
			overlay.Selected = m_HierarchyWindow->GetSelectedEntity();
			overlay.SelectionOutline = m_ShowSelectionOutline;
			overlay.Colliders = m_ShowColliders;
			m_SceneRenderer->RenderEditor(*m_EditorView, m_EditorCamera, overlay);
		}

		if (m_GameViewVisible || m_GameFullscreen)
			m_SceneHasCamera = m_SceneRenderer->RenderRuntime(*m_GameView);
	}

	void EditorLayer::OnImGuiRender()
	{
		UpdateFullscreenKeys();
		UpdateCaptureKeys();
		UpdateLaunchAutomation();

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
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
		EditorChrome::PushPlayModeTint(m_EditorSettings.PlayMode, IsPlaying());

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Editor dockspace", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
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
			EditorChrome::DrawBrand(m_EditorSettings.Style.GetHighlightColor());

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
				ImGui::Separator();
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					NewScene();
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
					Entity entity = m_ActiveScene->CreateEntity("Entity");
					m_HierarchyWindow->SetSelectedEntity(entity);
				}
				if (ImGui::MenuItem("Sprite"))
				{
					Entity entity = m_ActiveScene->CreateEntity("Sprite");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<SpriteRendererComponent>();
					m_HierarchyWindow->SetSelectedEntity(entity);
				}
				if (ImGui::MenuItem("Camera"))
				{
					Entity entity = m_ActiveScene->CreateEntity("Camera");
					entity.AddComponent<TransformComponent>();
					entity.AddComponent<CameraComponent>();
					m_HierarchyWindow->SetSelectedEntity(entity);
				}
				if (ImGui::MenuItem("Light"))
				{
					Entity entity = m_ActiveScene->CreateEntity("Light");
					entity.AddComponent<TransformComponent>();
					// entity.AddComponent<LightComponent>();
					m_HierarchyWindow->SetSelectedEntity(entity);
				}
				if (ImGui::BeginMenu("Meshes"))
				{
					if (ImGui::MenuItem("Box"))
					{
						Entity entity = m_ActiveScene->CreateEntity("Box");
						entity.AddComponent<TransformComponent>();
						auto x = entity.AddComponent<MeshComponent>();
						x.Shape = MeshShape::Box;
						m_HierarchyWindow->SetSelectedEntity(entity);
					}
					if (ImGui::MenuItem("Cylinder"))
					{
						Entity entity = m_ActiveScene->CreateEntity("Cylinder");
						entity.AddComponent<TransformComponent>();
						auto x = entity.AddComponent<MeshComponent>();
						x.Shape = MeshShape::Cylinder;
						m_HierarchyWindow->SetSelectedEntity(entity);
					}
					if (ImGui::MenuItem("Plane"))
					{
						Entity entity = m_ActiveScene->CreateEntity("Plane");
						entity.AddComponent<TransformComponent>();
						auto x = entity.AddComponent<MeshComponent>();
						x.Shape = MeshShape::Plane;
						m_HierarchyWindow->SetSelectedEntity(entity);
					}
					if (ImGui::MenuItem("Stairs"))
					{
						Entity entity = m_ActiveScene->CreateEntity("Stairs");
						entity.AddComponent<TransformComponent>();
						auto x = entity.AddComponent<MeshComponent>();
						x.Shape = MeshShape::Stairs;
						m_HierarchyWindow->SetSelectedEntity(entity);
					}
					if (ImGui::MenuItem("Ramp"))
					{
						Entity entity = m_ActiveScene->CreateEntity("Ramp");
						entity.AddComponent<TransformComponent>();
						auto x = entity.AddComponent<MeshComponent>();
						x.Shape = MeshShape::Ramp;
						m_HierarchyWindow->SetSelectedEntity(entity);
					}


					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}


			DrawPlayControls();

			// The waterline: the accent while editing, the play-mode tint while playing.
			const bool tinted = IsPlaying() && m_EditorSettings.PlayMode.Tint;
			EditorChrome::DrawMenuBarWaterline(tinted ? m_EditorSettings.PlayMode.TintColor : m_EditorSettings.Style.GetHighlightColor());

			ImGui::EndMenuBar();
		}

		m_HierarchyWindow->OnImguiRender(m_ActiveScene);
		// Picking an entity in the hierarchy takes the inspector back from an asset.
		const Entity selectedEntity = m_HierarchyWindow->GetSelectedEntity();
		if ((entt::entity)selectedEntity != m_LastSelectedEntity && (bool)selectedEntity)
		{
			m_InspectedAsset.clear();
		}
		m_LastSelectedEntity = (entt::entity)selectedEntity;
		m_InspectorWindow->OnImguiRender(selectedEntity, m_InspectedAsset);
		m_SpriteEditorWindow->OnImguiRender();
		m_SpriteAnimationWindow->OnImguiRender();
		m_StatsWindow->OnImguiRender(m_ActiveScene);
		m_RenderSettingsWindow->OnImguiRender(m_EditorScene.get(), m_ActiveScene.get());
		m_ProjectSettingsWindow->OnImguiRender(m_EditorScenePath);
		m_EditorSettingsWindow->OnImguiRender(m_EditorSettings);

		m_ProjectWindow->OnImguiRender(m_ActiveScene);
		// After the Project window, which it docks next to the first time.
		m_ConsoleWindow->OnImguiRender(m_EditorSettings.Console);

		DrawEditorWindow();
		DrawGameWindow();

		// A window opened or closed (View menu or its close button): remember it now rather than
		// only on close, so it also holds when the editor is stopped from the debugger.
		if (StoreOpenWindows())
		{
			EditorSettingsSerializer::SaveSession(m_EditorSettings.Session, EditorPaths::GetEditorSettingsPath());
		}

		ImGui::End();

		// Over everything else, so it is drawn last.
		if (m_GameFullscreen)
		{
			DrawGameFullscreen();
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
			ImGui::SetCursorPosX(centre);

		const ImVec4 accent = m_EditorSettings.Style.GetHighlightColor();
		const ImVec4 playColor = m_EditorSettings.PlayMode.Tint ? m_EditorSettings.PlayMode.TintColor : accent;

		// Play turns into Stop (in the play-mode colour) while the game runs.
		const bool playing = IsPlaying();
		if (EditorChrome::IconButton("##PlayStop", playing ? ToolbarIcon::Stop : ToolbarIcon::Play, buttonSize, playing, playColor,
			playing ? "Stop (back to the editor scene)" : "Play (runs a copy of the scene)"))
		{
			if (playing)
				StopScene();
			else
				PlayScene();
		}

		ImGui::BeginDisabled(playing == false);

		if (EditorChrome::IconButton("##Pause", m_Paused ? ToolbarIcon::Play : ToolbarIcon::Pause, buttonSize, m_Paused, accent,
			m_Paused ? "Resume" : "Pause"))
			m_Paused = m_Paused == false;

		ImGui::BeginDisabled(m_Paused == false);
		if (EditorChrome::IconButton("##Step", ToolbarIcon::Step, buttonSize, false, accent, "Step one frame"))
			m_StepFrames++;
		ImGui::EndDisabled();

		ImGui::EndDisabled();
	}

	void EditorLayer::DrawEditorWindow()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		m_EditorViewVisible = ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_MenuBar);
		m_EditorViewHovered = false;

		if (m_EditorViewVisible)
		{
			if (ImGui::BeginMenuBar())
			{
				DrawEditorCameraMenu();

				if (ImGui::BeginMenu("Overlays"))
				{
					ImGui::MenuItem("Selection Outline", nullptr, &m_ShowSelectionOutline);
					ImGui::MenuItem("Collider Outlines", nullptr, &m_ShowColliders);
					ImGui::Separator();
					if (ImGui::MenuItem("Focus Selected", "F", false, (bool)m_HierarchyWindow->GetSelectedEntity()))
						FocusEntity(m_HierarchyWindow->GetSelectedEntity());

					ImGui::EndMenu();
				}

				DrawCaptureMenu();

				ImGui::EndMenuBar();
			}

			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x >= 1.0f && size.y >= 1.0f)
			{
				ResizeEditorView((uint32_t)size.x, (uint32_t)size.y);
			}

			// Camera projections flip Y for Vulkan, so the image is shown the right way up as is.
			ImGui::Image((ImTextureID)m_EditorView->GetImGuiTexture(), size);
			m_EditorViewHovered = ImGui::IsItemHovered();
			DrawEditorCameraHint(ImGui::GetItemRectMin());
			UpdateClickSelection(ImGui::GetItemRectMin(), size);
			AcceptAssetDrop();

			// F: look at the selected entity (edit mode only; play mode scripts may use F).
			const bool focusKey = Input::IsKeyPressed(KeyCode::F);
			if (focusKey && m_FocusKeyDown == false && m_EditorViewHovered && IsPlaying() == false && ImGui::GetIO().WantTextInput == false)
				FocusEntity(m_HierarchyWindow->GetSelectedEntity());
			m_FocusKeyDown = focusKey;
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::DrawEditorCameraMenu()
	{
		const bool orthographic = m_EditorCamera.GetProjectionType() == ProjectionType::Orthographic;

		if (ImGui::BeginMenu("Camera"))
		{
			if (ImGui::MenuItem("2D", "Orthographic, top-down", orthographic))
			{
				m_EditorCamera.SetProjectionType(ProjectionType::Orthographic);
			}
			if (ImGui::MenuItem("3D", "Perspective", orthographic == false))
			{
				m_EditorCamera.SetProjectionType(ProjectionType::Perspective);
			}

			ImGui::Separator();
			// The standard views turn the camera around what it looks at (and switch to 3D).
			struct ViewItem
			{
				const char* Label;
				EditorCameraView View;
			};
			static const ViewItem views[] = {
				{ "Top", EditorCameraView::Top }, { "Diorama (50 degrees)", EditorCameraView::Diorama },
				{ "Front (looking north)", EditorCameraView::Front }, { "Back (looking south)", EditorCameraView::Back },
				{ "Left (looking east)", EditorCameraView::Left }, { "Right (looking west)", EditorCameraView::Right }
			};
			for (const ViewItem& item : views)
			{
				if (ImGui::MenuItem(item.Label))
				{
					m_EditorCamera.SetView(item.View);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset View"))
			{
				m_EditorCamera.Reset();
			}

			ImGui::Separator();
			ImGui::TextDisabled("Alt+LMB orbit, MMB pan, wheel / Alt+RMB zoom");
			ImGui::TextDisabled("3D: hold RMB to fly: mouse look, WASD move, Q/E down/up,");
			ImGui::TextDisabled("Shift faster, wheel changes the speed");

			ImGui::EndMenu();
		}

		// A one-click switch next to the menu: shows the mode you're in, switches to the other.
		if (ImGui::MenuItem(orthographic ? "2D" : "3D"))
		{
			m_EditorCamera.SetProjectionType(orthographic ? ProjectionType::Perspective : ProjectionType::Orthographic);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(orthographic ? "2D view (orthographic, top-down). Click for 3D" : "3D view (perspective). Click for 2D");
		}
	}

	void EditorLayer::DrawEditorCameraHint(const ImVec2& imageMin)
	{
		// While flying: the speed in the view's corner, so the wheel's change is visible.
		if (m_EditorCamera.IsFlying() == false)
		{
			return;
		}

		char text[64];
		std::snprintf(text, sizeof(text), "Fly %.1f m/s", m_EditorCamera.GetFlySpeed());
		const ImVec2 position{ imageMin.x + 8.0f, imageMin.y + 6.0f };
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddText(ImVec2(position.x + 1.0f, position.y + 1.0f), ImGui::GetColorU32(ImGuiCol_WindowBg), text);
		drawList->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), text);
	}

	void EditorLayer::UpdateClickSelection(const ImVec2& imageMin, const ImVec2& imageSize)
	{
		// The answer to an earlier click: the entity drawn under it, or nothing (clears the selection).
		int32_t picked = -1;
		if (m_EditorView->TakeEntityIDResult(picked) && m_ActiveScene != nullptr)
		{
			const entt::entity handle = (entt::entity)(uint32_t)picked;
			if (picked >= 0 && m_ActiveScene->m_Registry.valid(handle))
			{
				m_HierarchyWindow->SetSelectedEntity(Entity(handle, m_ActiveScene.get()));
			}
			else
			{
				m_HierarchyWindow->ClearSelection();
			}
		}

		// A click is a press and release in the image without moving: a drag (orbit with Alt, a box
		// later, an asset dropped) is not one, nor is anything with Alt held (camera controls).
		const ImGuiIO& io = ImGui::GetIO();
		const bool alt = io.KeyAlt;
		if (m_EditorViewHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && alt == false && ImGui::GetDragDropPayload() == nullptr)
		{
			m_ClickPending = true;
			m_ClickStart = io.MousePos;
		}
		if (m_ClickPending == false || ImGui::IsMouseReleased(ImGuiMouseButton_Left) == false)
		{
			return;
		}
		m_ClickPending = false;

		const float dx = io.MousePos.x - m_ClickStart.x;
		const float dy = io.MousePos.y - m_ClickStart.y;
		if (m_EditorViewHovered == false || alt || dx * dx + dy * dy > 16.0f || imageSize.x < 1.0f || imageSize.y < 1.0f)
		{
			return;
		}

		// ImGui pixels to the view's own (the same unless the view hasn't caught up with a resize yet).
		const float u = (io.MousePos.x - imageMin.x) / imageSize.x;
		const float v = (io.MousePos.y - imageMin.y) / imageSize.y;
		if (u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f)
		{
			return;
		}
		const uint32_t x = std::min((uint32_t)(u * (float)m_EditorView->GetWidth()), m_EditorView->GetWidth() - 1);
		const uint32_t y = std::min((uint32_t)(v * (float)m_EditorView->GetHeight()), m_EditorView->GetHeight() - 1);
		m_EditorView->RequestEntityIDReadback(x, y);
	}

	void EditorLayer::FocusEntity(Entity entity)
	{
		if (entity.IsValid() == false || entity.HasComponent<TransformComponent>() == false)
		{
			return;
		}

		const TransformComponent& transform = entity.GetComponent<TransformComponent>();
		const float extent = std::max({ std::abs(transform.Scale.x), std::abs(transform.Scale.y), 1.0f });
		m_EditorCamera.Frame(transform.Translation, extent);
	}

	void EditorLayer::DrawGameWindow()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		m_GameViewVisible = ImGui::Begin("Game", nullptr, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::MenuItem("Full screen", "F11", m_GameFullscreen))
			{
				SetGameFullscreen(m_GameFullscreen == false);
			}
			if (ImGui::BeginMenu("Options"))
			{
				bool changed = ImGui::MenuItem("Full screen covers the monitor", nullptr, &m_EditorSettings.PlayMode.FullscreenCoversMonitor);
				changed |= ImGui::MenuItem("Leave full screen on Stop", nullptr, &m_EditorSettings.PlayMode.LeaveFullscreenOnStop);
				if (changed)
				{
					EditorSettingsSerializer::Save(m_EditorSettings, EditorPaths::GetEditorSettingsPath());
				}
				ImGui::EndMenu();
			}
			DrawCaptureMenu();
			ImGui::EndMenuBar();
		}

		if (m_GameViewVisible && m_GameFullscreen)
		{
			ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8.0f, ImGui::GetCursorPosY() + 8.0f));
			ImGui::TextDisabled("The game is in full screen. F11 or Escape to leave.");
		}
		else if (m_GameViewVisible)
		{
			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x >= 1.0f && size.y >= 1.0f)
			{
				ResizeGameView((uint32_t)size.x, (uint32_t)size.y);
			}

			const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
			ImGui::Image((ImTextureID)m_GameView->GetImGuiTexture(), size);
			if (IsPlaying() && m_EditorSettings.PlayMode.Tint)
				EditorChrome::OutlineLastItem(m_EditorSettings.PlayMode.TintColor, 2.0f);
			AcceptAssetDrop();

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImVec2 textPosition = { imagePosition.x + 8.0f, imagePosition.y + 8.0f };
			if (m_SceneHasCamera == false)
				drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255), "No primary camera in the scene. Add a Camera component and tick Primary.");
			else if (IsPlaying() && m_Paused)
				drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255), "Paused");
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::SetGameFullscreen(bool fullscreen)
	{
		if (fullscreen == m_GameFullscreen)
		{
			return;
		}

		m_GameFullscreen = fullscreen;
		if (fullscreen)
		{
			m_FocusFullscreen = true;
			m_FullscreenHintTime = 2.5f;
			// Only when the window isn't full screen already, so leaving gives back what the user had.
			if (m_EditorSettings.PlayMode.FullscreenCoversMonitor && Application::Get().GetWindow().IsFullscreen() == false)
			{
				m_PendingWindowFullscreen = 1;
			}
		}
		else if (m_WindowFullscreenByGame || m_PendingWindowFullscreen == 1)
		{
			m_PendingWindowFullscreen = 0;
		}
	}

	void EditorLayer::UpdateFullscreenKeys()
	{
		// Polled like the editor's other keys; F11 toggles, Escape only leaves.
		const bool f11 = Input::IsKeyPressed(KeyCode::F11);
		const bool escape = Input::IsKeyPressed(KeyCode::Escape);
		if (f11 && m_FullscreenKeyDown == false)
		{
			SetGameFullscreen(m_GameFullscreen == false);
		}
		else if (escape && m_EscapeKeyDown == false && m_GameFullscreen)
		{
			SetGameFullscreen(false);
		}
		m_FullscreenKeyDown = f11;
		m_EscapeKeyDown = escape;
	}

	void EditorLayer::DrawGameFullscreen()
	{
		// One undecorated window over the main viewport, in front of the dockspace and every docked window.
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		if (m_FocusFullscreen)
		{
			ImGui::SetNextWindowFocus();
			m_FocusFullscreen = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::Begin("##GameFullscreen", nullptr, flags);
		ImGui::PopStyleVar(3);

		const ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x >= 1.0f && size.y >= 1.0f)
		{
			ResizeGameView((uint32_t)size.x, (uint32_t)size.y);
		}

		const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)m_GameView->GetImGuiTexture(), size);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 textPosition = { imagePosition.x + 12.0f, imagePosition.y + 12.0f };
		if (m_SceneHasCamera == false)
		{
			drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255), "No primary camera in the scene. Add a Camera component and tick Primary.");
		}
		else if (IsPlaying() && m_Paused)
		{
			drawList->AddText(textPosition, IM_COL32(255, 200, 80, 255), "Paused");
		}

		// How to get out, for a moment after going in.
		if (m_FullscreenHintTime > 0.0f)
		{
			const float alpha = std::min(1.0f, m_FullscreenHintTime / 0.5f);
			const char* hint = "Full screen: F11 or Escape to leave";
			const ImVec2 textSize = ImGui::CalcTextSize(hint);
			const ImVec2 position = { imagePosition.x + 0.5f * (size.x - textSize.x), imagePosition.y + 24.0f };
			drawList->AddRectFilled({ position.x - 12.0f, position.y - 6.0f }, { position.x + textSize.x + 12.0f, position.y + textSize.y + 6.0f },
				IM_COL32(0, 0, 0, (int)(160.0f * alpha)), 6.0f);
			drawList->AddText(position, IM_COL32(255, 255, 255, (int)(255.0f * alpha)), hint);
		}

		ImGui::End();
	}

	void EditorLayer::ResizeGameView(uint32_t width, uint32_t height)
	{
		if (m_CaptureGameRequested)
		{
			if (m_CaptureFixedSize)
			{
				width = CaptureWidth;
				height = CaptureHeight;
			}
			m_GameView->RequestCapture(m_LaunchStep == LaunchStep::Capturing ? m_Launch.Capture : MakeCapturePath("Game"));
			m_CaptureGameRequested = false;
		}

		// The game's cameras take their aspect ratio from the scene's viewport.
		m_GameView->SetSize(width, height);
		m_ActiveScene->OnViewportResize(width, height);
	}

	void EditorLayer::ResizeEditorView(uint32_t width, uint32_t height)
	{
		if (m_CaptureEditorRequested)
		{
			if (m_CaptureFixedSize)
			{
				width = CaptureWidth;
				height = CaptureHeight;
			}
			m_EditorView->RequestCapture(MakeCapturePath("Editor"));
			m_CaptureEditorRequested = false;
		}

		m_EditorView->SetSize(width, height);
		m_EditorCamera.SetViewportSize((float)width, (float)height);
	}

	void EditorLayer::DrawCaptureMenu()
	{
		if (ImGui::BeginMenu("Capture"))
		{
			if (ImGui::MenuItem("Capture Game View", "F9"))
			{
				m_CaptureGameRequested = true;
			}
			if (ImGui::MenuItem("Capture Editor View", "Shift+F9"))
			{
				m_CaptureEditorRequested = true;
			}

			ImGui::Separator();
			ImGui::MenuItem("Fixed Size (1280 x 720)", nullptr, &m_CaptureFixedSize);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Render the captured frame at 1280 x 720, whatever the window's size,\nso captures can be compared. Pause first so the scene stands still.");
			}

			ImGui::Separator();
			ImGui::TextDisabled("Saved in <project>/.hydro/captures");
			ImGui::EndMenu();
		}
	}

	void EditorLayer::UpdateCaptureKeys()
	{
		// F9: the Game view (also in game full screen); Shift+F9: the Editor view. Holding captures once.
		// Not F12: Windows reserves F12 for debuggers (UserDebuggerHotKey), so pressing it while a
		// debugger is attached breaks into the debugger.
		const bool captureKey = Input::IsKeyPressed(KeyCode::F9);
		if (captureKey && m_CaptureKeyDown == false && ImGui::GetIO().WantTextInput == false)
		{
			const bool shift = Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::RightShift);
			if (shift)
			{
				m_CaptureEditorRequested = true;
			}
			else
			{
				m_CaptureGameRequested = true;
			}
		}
		m_CaptureKeyDown = captureKey;
	}

	std::filesystem::path EditorLayer::MakeCapturePath(const std::string& viewName) const
	{
		const std::string sceneName = m_ActiveScene != nullptr && m_ActiveScene->GetName().empty() == false
			? m_ActiveScene->GetName() : std::string("Untitled");

		const auto now = std::chrono::system_clock::now();
		const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
		const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

		std::tm local = {};
#if defined(_WIN32)
		localtime_s(&local, &seconds);
#else
		localtime_r(&seconds, &local);
#endif
		char stamp[32] = {};
		std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
		char suffix[8] = {};
		std::snprintf(suffix, sizeof(suffix), "-%03lld", milliseconds);

		const std::string fileName = EditorPaths::ToFileName(sceneName) + "_" + viewName + "_" + stamp + suffix + ".png";
		return EditorPaths::GetCapturesDirectory() / fileName;
	}

	void EditorLayer::AcceptAssetDrop()
	{
		// Drop prefabs / scenes from the project window onto the image.
		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath))
				OnAssetDropped(droppedPath);

			ImGui::EndDragDropTarget();
		}
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::MouseScrolled && m_EditorViewVisible && m_EditorViewHovered)
		{
			m_EditorCamera.Zoom(static_cast<MouseScrolledEvent&>(e).GetYOffset());
			e.Handled = true;
		}
	}

	void EditorLayer::PlayScene()
	{
		if (IsPlaying())
			return;

		const Entity selectedBefore = m_HierarchyWindow->GetSelectedEntity();

		SceneTransition::Clear();
		SceneTransition::SetDepth(0);
		m_SuspendedScenes.clear();

		m_RuntimeScene = Scene::Copy(m_EditorScene);
		m_RuntimeScene->Init();
		m_RuntimeScene->OnRuntimeStart(); // builds the physics world

		m_ActiveScene = m_RuntimeScene;
		m_SceneRenderer->SetScene(m_ActiveScene);
		m_EditorView->InvalidateEntityIDRequests(); // a click on the old scene picks nothing in this one
		m_SceneState = SceneState::Play;
		m_Paused = false;
		m_StepFrames = 0;

		// The copy has the same UUIDs, so what was selected stays selected (the copy's entity).
		SelectSameEntity(selectedBefore, *m_ActiveScene);
	}

	void EditorLayer::SelectSameEntity(Entity previous, Scene& scene)
	{
		if (previous.IsValid() && previous.HasComponent<IDComponent>())
		{
			m_HierarchyWindow->SetSelectedEntity(scene.GetEntityByUUID(previous.GetUUID()));
		}
		else
		{
			m_HierarchyWindow->ClearSelection();
		}
	}

	void EditorLayer::StopScene()
	{
		if (IsPlaying() == false)
			return;

		const Entity selectedBefore = m_HierarchyWindow->GetSelectedEntity();

		m_SceneState = SceneState::Edit;
		m_Paused = false;
		m_StepFrames = 0;

		m_ActiveScene = m_EditorScene;
		m_SceneRenderer->SetScene(m_ActiveScene);
		m_EditorView->InvalidateEntityIDRequests(); // a click on the old scene picks nothing in this one

		// Back to the editor scene's entity with the same UUID; cleared if it only existed while playing
		// (made or destroyed by a script).
		SelectSameEntity(selectedBefore, *m_ActiveScene);

		// Destroying the copy runs its scripts' OnDestroy; the editor scene is as it was before Play.
		// Scenes pushed on top of it during Play go with it, the top one first.
		m_RuntimeScene->OnRuntimeStop();
		m_RuntimeScene = nullptr;
		while (m_SuspendedScenes.empty() == false)
		{
			m_SuspendedScenes.pop_back();
		}

		SceneTransition::Clear();
		SceneTransition::SetDepth(0);

		if (m_EditorSettings.PlayMode.LeaveFullscreenOnStop)
		{
			SetGameFullscreen(false);
		}
	}

	void EditorLayer::ProcessSceneTransition()
	{
		const SceneTransition::Request request = SceneTransition::Take();
		if (request.Kind == SceneTransition::Type::None || IsPlaying() == false)
		{
			return;
		}

		if (request.Kind == SceneTransition::Type::Push)
		{
			// Like OpenScene: load into a fresh scene, and only switch when that worked.
			const std::filesystem::path path = EditorPaths::GetScenesDirectory() / (request.SceneName + SceneSerializer::FileExtension);
			Ref<Scene> next = CreateRef<Scene>(request.SceneName, false);
			SceneSerializer serializer(next);
			if (serializer.Deserialize(path) == false)
			{
				HY_WARN("Scene push: could not load {}", path.string());
				return;
			}

			next->SetName(request.SceneName);
			next->Init();
			next->OnViewportResize(m_GameView->GetWidth(), m_GameView->GetHeight());

			m_SuspendedScenes.push_back(m_RuntimeScene);
			m_RuntimeScene = next;
		}
		else
		{
			if (m_SuspendedScenes.empty())
			{
				HY_WARN("Scene pop: there is no scene below this one");
				return;
			}

			// The top scene goes (its scripts' OnDestroy run), the one below carries on as it was.
			m_RuntimeScene = m_SuspendedScenes.back();
			m_SuspendedScenes.pop_back();
		}

		m_ActiveScene = m_RuntimeScene;
		m_ActiveScene->OnViewportResize(m_GameView->GetWidth(), m_GameView->GetHeight());
		m_SceneRenderer->SetScene(m_ActiveScene);
		m_EditorView->InvalidateEntityIDRequests(); // a click on the old scene picks nothing in this one
		m_HierarchyWindow->ClearSelection();
		SceneTransition::SetDepth((uint32_t)m_SuspendedScenes.size());
	}

	void EditorLayer::NewProject()
	{
		std::string directory = FileDialogs::OpenDirectory();

		if (directory.empty())
			return;

		// <directory>/<directory name>.hyproj, with the standard asset folders.
		const std::filesystem::path projectDirectory = directory;
		const std::string name = EditorPaths::ToFileName(projectDirectory.filename().string());
		Ref<Project> project = Project::New(projectDirectory / (name + Project::FileExtension));
		m_EditorSettings.Session.LastProject = projectDirectory / (name + Project::FileExtension);

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
			return false;

		OpenProject(filepath);
		
		return true;
	}
	
	void EditorLayer::OpenProject(const std::filesystem::path& path)
	{
		Ref<Project> project = Project::Load(path);
		if (project == nullptr)
			return; // Already logged; the current project stays open.

		std::error_code pathError;
		m_EditorSettings.Session.LastProject = std::filesystem::absolute(path, pathError);
		Renderer::SetSettings(project->GetConfig().Renderer);
		// Gives every asset file without a handle one (written into the file or a .hymeta next to it).
		AssetManager::OnProjectOpened(project->GetAssetDirectory());

		// A project file from before handles names its start scene by path: turn that into a handle now.
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
			return;

		const std::filesystem::path startScene = AssetManager::GetAbsolutePath(config.StartScene);
		std::error_code ec;
		if (AssetManager::IsAssetHandleValid(config.StartScene) && std::filesystem::exists(startScene, ec))
			OpenScene(startScene);
		else
			HY_WARN("Project start scene (asset handle {}) is missing", (uint64_t)config.StartScene);
	}
	
	void EditorLayer::SaveProject()
	{
		Ref<Project> project = Project::GetActive();
		if (project == nullptr)
			return;

		project->GetConfig().Renderer = Renderer::GetSettings();
		Project::SaveActive();
	}

	void EditorLayer::LoadEditorSettings()
	{
		// No file yet (first run) or an unreadable one: the defaults (Hydro theme).
		EditorSettings settings;
		if (EditorSettingsSerializer::Load(EditorPaths::GetEditorSettingsPath(), settings) == false)
			HY_INFO("No editor settings at {}; using the defaults", EditorPaths::GetEditorSettingsPath().string());

		m_EditorSettings = settings;
		m_EditorSettings.Style.Apply();
		m_EditorCamera.SetSettings(m_EditorSettings.Camera);
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

	// Called after an image's import settings were saved, before the window that saved them reads the image
	// again: the AssetManager reloads the texture (every holder of its handle gets the new one), the animation
	// sets built on it are read again, and sprites keep showing the same sprite by name.
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

		for (const Ref<Scene>& scene : { m_EditorScene, m_RuntimeScene })
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

	// Called after the Animation window saved `path`: the set is read again and every animator on it keeps its
	// clip when that clip still exists (by name), and starts over otherwise.
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

		for (const Ref<Scene>& scene : { m_EditorScene, m_RuntimeScene })
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
		const std::map<std::string, bool>& saved = m_EditorSettings.Session.OpenWindows;
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
		std::map<std::string, bool>& saved = m_EditorSettings.Session.OpenWindows;
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
		const EditorSession session = m_EditorSettings.Session;
		LoadEditorSettings();
		m_EditorSettings.Session = session;
	}

	void EditorLayer::UpdateAutosave(float deltaTime)
	{
		const AutosaveSettings& autosave = m_EditorSettings.Autosave;
		if (autosave.Enabled == false || IsPlaying() || m_EditorScenePath.empty())
		{
			m_AutosaveTimer = 0.0f;
			return;
		}

		m_AutosaveTimer += deltaTime;
		if (m_AutosaveTimer < autosave.IntervalMinutes * 60.0f)
			return;

		m_AutosaveTimer = 0.0f;
		SaveScene();
		HY_INFO("Autosaved {}", m_EditorScenePath.filename().string());
	}

	void EditorLayer::OpenStartupProject()
	{
		const EditorStartupSettings& startup = m_EditorSettings.Startup;
		// Copy: opening a project or scene updates the session.
		const EditorSession session = m_EditorSettings.Session;

		std::error_code ec;
		if (startup.ReopenLastProject && session.LastProject.empty() == false && std::filesystem::exists(session.LastProject, ec))
			OpenProject(session.LastProject);

		if (Project::GetActive() == nullptr)
			OpenDefaultProject();

		// The project opened its start scene; the last scene replaces it if it was another one.
		if (startup.ReopenLastScene && session.LastScene.empty() == false && session.LastScene != m_EditorScenePath &&
			std::filesystem::exists(session.LastScene, ec))
			OpenScene(session.LastScene);
	}

	void EditorLayer::SaveEditorSettings()
	{
		EditorSettingsSerializer::Save(m_EditorSettings, EditorPaths::GetEditorSettingsPath());
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
					return;
			}
		}

		// None found (or it failed to load): an unsaved project in the default folder, so the editor
		// always has one. File > Save Project writes it.
		Ref<Project> project = Project::New(directory / "Sandbox.hyproj");
		AssetManager::OnProjectOpened(project->GetAssetDirectory());
		HY_INFO("No project found in {}; started an unsaved one", directory.string());
	}

	void EditorLayer::SetEditorScene(const Ref<Scene>& scene)
	{
		StopScene();

		m_EditorScene = scene;
		m_EditorScene->Init();
		m_EditorScene->OnViewportResize(m_GameView->GetWidth(), m_GameView->GetHeight());
		m_ActiveScene = m_EditorScene;
		m_SceneRenderer->SetScene(m_ActiveScene);
		m_EditorView->InvalidateEntityIDRequests(); // a click on the old scene picks nothing in this one
		m_HierarchyWindow->ClearSelection();
	}

	void EditorLayer::NewScene()
	{
		SetEditorScene(CreateRef<Scene>("Untitled", true));
		CreateDefaultCamera();
		m_EditorScenePath.clear();
		m_EditorSettings.Session.LastScene.clear();
	}

	void EditorLayer::CreateDefaultCamera()
	{
		Entity camera = m_EditorScene->CreateEntity("Camera");
		camera.AddComponent<TransformComponent>();
		camera.AddComponent<CameraComponent>();
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath = FileDialogs::OpenFile("Hydro Scene (*.hyscene)\0*.hyscene\0");

		if (filepath.empty())
			return;

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
			return;

		// The file name is the scene's name, so a scene renamed on disk shows its new name.
		newScene->SetName(path.stem().string());
		SetEditorScene(newScene);
		m_EditorScenePath = path;

		std::error_code ec;
		m_EditorSettings.Session.LastScene = std::filesystem::absolute(path, ec);
	}

	void EditorLayer::SaveScene()
	{
		// First save of a new scene goes to <project>/Assets/Scenes/<SceneName>.hyscene, numbered
		// ("Untitled 2") so it never overwrites another scene. Rename it in the Project window.
		if (m_EditorScenePath.empty())
		{
			const std::filesystem::path directory = EditorPaths::GetScenesDirectory();
			const std::string baseName = EditorPaths::ToFileName(m_EditorScene->GetName());
			std::string name = baseName;
			std::error_code ec;
			for (int i = 2; std::filesystem::exists(directory / (name + SceneSerializer::FileExtension), ec); i++)
			{
				name = baseName + " " + std::to_string(i);
			}

			std::filesystem::create_directories(directory, ec);
			m_EditorScene->SetName(name);
			m_EditorScenePath = directory / (name + SceneSerializer::FileExtension);
		}

		// Always the editor scene: what happens while playing is not saved.
		SceneSerializer serializer(m_EditorScene);
		if (serializer.Serialize(m_EditorScenePath))
		{
			std::error_code ec;
			m_EditorSettings.Session.LastScene = std::filesystem::absolute(m_EditorScenePath, ec);
		}
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("Hydro Scene (*.hyscene)\0*.hyscene\0");

		if (filepath.empty())
			return;

		std::filesystem::path path = filepath;
		if (path.extension().string() != SceneSerializer::FileExtension)
			path += SceneSerializer::FileExtension;

		// Name the scene after its file.
		m_EditorScene->SetName(path.stem().string());

		SceneSerializer serializer(m_EditorScene);
		if (serializer.Serialize(path))
		{
			m_EditorScenePath = path;

			std::error_code ec;
			m_EditorSettings.Session.LastScene = std::filesystem::absolute(path, ec);
		}
	}

	void EditorLayer::OnPathRenamed(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		// The open scene and the last scene are remembered by path (the last scene is an editor setting and can
		// be outside any project), so they follow a rename here. The project's start scene, textures and
		// .hyanim files are referenced by asset handle and follow on their own.
		if (PathFollower::Follow(m_EditorScenePath, from, to))
		{
			m_EditorScene->SetName(m_EditorScenePath.stem().string());
		}
		PathFollower::Follow(m_EditorSettings.Session.LastScene, from, to);
		PathFollower::Follow(m_InspectedAsset, from, to);
		m_SpriteEditorWindow->OnAssetMoved(from, to);
		m_SpriteAnimationWindow->OnAssetMoved(from, to);
	}

	void EditorLayer::OnPathDeleted(const std::filesystem::path& path)
	{
		// What was open for editing is gone; saving it would bring the file back.
		if (PathFollower::Follow(m_InspectedAsset, path, path))
		{
			m_InspectedAsset.clear();
		}
		m_SpriteEditorWindow->OnAssetDeleted(path);
		m_SpriteAnimationWindow->OnAssetDeleted(path);
	}

	void EditorLayer::OnAssetDropped(const std::filesystem::path& path)
	{
		const std::string extension = path.extension().string();

		if (extension == PrefabSerializer::FileExtension)
		{
			if (Entity entity = PrefabSerializer::Instantiate(*m_ActiveScene, path))
				m_HierarchyWindow->SetSelectedEntity(entity);
		}
		else if (extension == SceneSerializer::FileExtension)
		{
			OpenScene(path);
		}
		else if (GetAssetTypeFromExtension(path) == AssetType::Mesh)
		{
			// A model: an entity with a Mesh component at the editor camera's focus, on the ground, using the
			// materials the import made (no slots of its own).
			const AssetHandle handle = AssetManager::FindHandle(path);
			const Ref<Mesh> model = (uint64_t)handle != 0 ? AssetManager::GetAsset<Mesh>(handle) : nullptr;
			if (model == nullptr)
			{
				return;
			}

			Entity entity = m_ActiveScene->CreateEntity(path.stem().u8string());
			const glm::vec3 focus = m_EditorCamera.GetFocalPoint();
			entity.AddComponent<TransformComponent>().Translation = { focus.x, focus.y, 0.0f };
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.Model = handle;
			mesh.Materials.clear();
			m_HierarchyWindow->SetSelectedEntity(entity);
		}
	}
}
