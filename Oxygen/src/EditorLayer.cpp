#include "EditorLayer.h"
#include <iostream>

#include <imgui/imgui.h>
#include <string>

#include "Hydro/Utils/PlatformUtils.h"
#include "EditorPaths.h"
#include "EditorWindows/AssetDragDrop.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
	// Per-star animation data for the demo galaxy (plain entt component).
	struct GalaxyStar
	{
		float Radius;        // 0..1, scaled to the viewport each frame
		float Angle;         // radians
		float AngularSpeed;  // radians / second (inner stars orbit faster)
		float TwinklePhase;
		glm::vec3 BaseColor;
		bool Bright;
	};

	// 20k quads per batch -> 50k stars = 3 batches / 3 draw calls (20k + 20k + 10k).
	constexpr uint32_t GalaxyStarCount = 50000;
	// World units: the galaxy fits the default 10-unit-high camera view.
	constexpr float GalaxyRadius = 4.0f;
	constexpr float StarSizeScale = 0.012f;
	constexpr int GalaxyArms = 4;
	constexpr float Pi = 3.14159265f;

	glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }

	// Warm core -> pink mid -> blue rim.
	glm::vec3 GalaxyColor(float radius)
	{
		const glm::vec3 core(1.00f, 0.88f, 0.62f);
		const glm::vec3 mid(0.92f, 0.45f, 0.85f);
		const glm::vec3 rim(0.35f, 0.55f, 1.00f);
		return radius < 0.35f ? Lerp(core, mid, radius / 0.35f)
		                      : Lerp(mid, rim, std::min((radius - 0.35f) / 0.65f, 1.0f));
	}
}

namespace Hydro
{
	void EditorLayer::OnAttach()
	{
		m_Scene = CreateRef<Scene>("Scene", false);
		m_SceneRenderer = CreateScope<SceneRenderer>(m_Scene);
		m_Scene->Init();
		m_HierarchyWindow = CreateRef<HierachyWindow>();
		m_StatsWindow = CreateRef<StatsWindow>();
		m_InspectorWindow = CreateRef<InspectorWindow>();
		m_ProjectWindow = CreateRef<ProjectWindow>();
		m_ProjectWindow->OnOpenScene = [this](const std::filesystem::path& path) { OpenScene(path); };
		m_ProjectWindow->OnEntityCreated = [this](Entity entity) { m_HierarchyWindow->SetSelectedEntity(entity); };
		m_RenderSettingsWindow = CreateRef<RenderSettingsWindow>();
		m_RenderSettingsWindow->OnSaveProject = [this]() { SaveProject(); };
		m_ProjectSettingsWindow = CreateRef<ProjectSettingsWindow>();
		m_ProjectSettingsWindow->OnSaveProject = [this]() { SaveProject(); };

		CreateGalaxy();
		CreateDefaultCamera();

		// After the demo scene: a project with a start scene replaces it.
		OpenDefaultProject();
	}

	void EditorLayer::CreateGalaxy()
	{
		// One tagged entity so the galaxy shows up in the hierarchy. The stars themselves get
		// no Tag/ID: the hierarchy window builds a tree node for every tagged entity each frame,
		// which would be far too slow for 50k entities.
		m_Scene->CreateEntity("Galaxy (" + std::to_string(GalaxyStarCount) + " stars)");

		entt::registry& registry = m_Scene->m_Registry;

		std::mt19937 rng(1337);
		std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
		std::normal_distribution<float> gauss(0.0f, 1.0f);

		for (uint32_t i = 0; i < GalaxyStarCount; i++)
		{
			float radius, angle;

			if (uniform(rng) < 0.18f)
			{
				// Dense core.
				radius = std::min(std::abs(gauss(rng)) * 0.11f, 1.0f);
				angle = uniform(rng) * 2.0f * Pi;
			}
			else
			{
				// Spiral arms: the angle offset grows with radius, plus some scatter.
				int arm = (int)(uniform(rng) * GalaxyArms) % GalaxyArms;
				radius = 0.06f + 0.94f * std::pow(uniform(rng), 0.85f);
				radius = std::clamp(radius + gauss(rng) * 0.015f, 0.0f, 1.0f);
				angle = arm * (2.0f * Pi / GalaxyArms) + radius * 5.5f + gauss(rng) * (0.12f + 0.18f * radius);
			}

			const bool bright = uniform(rng) < 0.01f;
			const float size = StarSizeScale * (bright ? 4.0f + uniform(rng) * 3.0f : 1.5f + uniform(rng) * 2.0f);
			const glm::vec3 color = GalaxyColor(radius) * (0.55f + 0.45f * uniform(rng));

			GalaxyStar star;
			star.Radius = radius;
			star.Angle = angle;
			star.AngularSpeed = 0.10f * (1.0f + 0.35f / (radius + 0.25f));
			star.TwinklePhase = uniform(rng) * 2.0f * Pi;
			star.BaseColor = bright ? glm::vec3(1.0f) : color;
			star.Bright = bright;

			entt::entity entity = registry.create();
			registry.emplace<TransformComponent>(entity, glm::vec3(0.0f), glm::vec3(size, size, 1.0f));
			registry.emplace<SpriteRendererComponent>(entity, glm::vec4(star.BaseColor, 1.0f));
			registry.emplace<GalaxyStar>(entity, star);
		}
	}

	void EditorLayer::UpdateGalaxy(float deltaTime)
	{
		m_Time += deltaTime;

		// Centred on the origin; Y is squashed a little so it reads as a tilted disc.

		auto view = m_Scene->m_Registry.view<GalaxyStar, TransformComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			GalaxyStar& star = view.get<GalaxyStar>(entity);
			TransformComponent& transform = view.get<TransformComponent>(entity);

			star.Angle += star.AngularSpeed * deltaTime;

			const float r = star.Radius * GalaxyRadius;
			transform.Translation.x = std::cos(star.Angle) * r;
			transform.Translation.y = std::sin(star.Angle) * r * 0.78f;

			if (star.Bright)
			{
				SpriteRendererComponent& sprite = view.get<SpriteRendererComponent>(entity);
				const float twinkle = 0.65f + 0.35f * std::sin(m_Time * 3.0f + star.TwinklePhase);
				sprite.Color = glm::vec4(star.BaseColor * twinkle, 1.0f);
			}
		}
	}

	void EditorLayer::OnDetach()
	{
		// Release everything OnAttach created, here rather than in the destructor:
		// Application::Shutdown detaches layers while the renderer is still alive.
		m_ProjectSettingsWindow = nullptr;
		m_RenderSettingsWindow = nullptr;
		m_ProjectWindow = nullptr;
		m_InspectorWindow = nullptr;
		m_StatsWindow = nullptr;
		m_HierarchyWindow = nullptr;
		m_SceneRenderer.reset();
		m_Scene = nullptr;

		// The editor opened the project, so it closes it.
		Project::Close();
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{
		// deltaTime comes from the Time subsystem (Time::GetDeltaTime()), passed in by Application::Run.
		// 1. Engine systems. The editor is always editing (there's no play mode yet), so scripts don't run.
		m_Scene->OnUpdateEditor(deltaTime);
		// 2. Application systems.
		UpdateGalaxy(deltaTime);

		// 3. Render submission, through the camera the viewport looks through.
		if (m_ViewportCamera == ViewportCamera::Editor)
		{
			m_EditorCamera.Update(m_ViewportHovered);
			m_SceneRenderer->RenderEditor(m_EditorCamera);
		}
		else
		{
			m_SceneHasCamera = m_SceneRenderer->RenderRuntime();
		}
	}

	void EditorLayer::OnImGuiRender()
	{
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
				ImGui::MenuItem("Stats", nullptr, &m_StatsWindow->IsOpen());
				ImGui::MenuItem("Render Settings", nullptr, &m_RenderSettingsWindow->IsOpen());
				ImGui::MenuItem("Project Settings", nullptr, &m_ProjectSettingsWindow->IsOpen());
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_HierarchyWindow->OnImguiRender(m_Scene);
		m_InspectorWindow->OnImguiRender(m_HierarchyWindow->GetSelectedEntity());
		m_StatsWindow->OnImguiRender(m_Scene);
		m_RenderSettingsWindow->OnImguiRender();
		m_ProjectSettingsWindow->OnImguiRender(m_EditorScenePath);

		m_ProjectWindow->OnImguiRender(m_Scene);

		DrawViewport();

		ImGui::End();
	}

	void EditorLayer::DrawViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("ViewPort", nullptr, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Camera"))
			{
				if (ImGui::MenuItem("Editor Camera", nullptr, m_ViewportCamera == ViewportCamera::Editor))
					m_ViewportCamera = ViewportCamera::Editor;
				if (ImGui::MenuItem("Scene Camera (Primary)", nullptr, m_ViewportCamera == ViewportCamera::Scene))
					m_ViewportCamera = ViewportCamera::Scene;

				ImGui::Separator();

				const bool editorCamera = m_ViewportCamera == ViewportCamera::Editor;
				const bool orthographic = m_EditorCamera.GetProjectionType() == ProjectionType::Orthographic;
				if (ImGui::MenuItem("2D (Orthographic)", nullptr, orthographic, editorCamera))
					m_EditorCamera.SetProjectionType(orthographic ? ProjectionType::Perspective : ProjectionType::Orthographic);
				if (ImGui::MenuItem("Reset View", nullptr, false, editorCamera))
					m_EditorCamera.Reset();

				ImGui::Separator();
				ImGui::TextDisabled("Alt+LMB orbit, MMB pan, Alt+RMB / wheel zoom");

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImVec2 viewPortSize = ImGui::GetContentRegionAvail();
		if (viewPortSize.x >= 1.0f && viewPortSize.y >= 1.0f)
			m_EditorCamera.SetViewportSize(viewPortSize.x, viewPortSize.y);

		const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
		m_SceneRenderer->RenderCompositeImage(viewPortSize.x, viewPortSize.y);
		m_ViewportHovered = ImGui::IsItemHovered();

		// Drop prefabs / scenes from the project window onto the viewport.
		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath))
				OnAssetDropped(droppedPath);

			ImGui::EndDragDropTarget();
		}

		if (m_ViewportCamera == ViewportCamera::Scene && m_SceneHasCamera == false)
		{
			ImGui::GetWindowDrawList()->AddText({ imagePosition.x + 8.0f, imagePosition.y + 8.0f }, IM_COL32(255, 200, 80, 255),
				"No primary camera in the scene. Add a Camera component and tick Primary.");
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::MouseScrolled && m_ViewportHovered && m_ViewportCamera == ViewportCamera::Editor)
		{
			m_EditorCamera.Zoom(static_cast<MouseScrolledEvent&>(e).GetYOffset());
			e.Handled = true;
		}
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

		std::error_code ec;
		std::filesystem::create_directories(project->GetAssetDirectory() / "Scenes", ec);
		std::filesystem::create_directories(project->GetAssetDirectory() / "Prefabs", ec);

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

		Renderer::SetSettings(project->GetConfig().Renderer);

		const std::filesystem::path startScene = project->GetStartScenePath();
		if (startScene.empty())
			return;

		std::error_code ec;
		if (std::filesystem::exists(startScene, ec))
			OpenScene(startScene);
		else
			HY_WARN("Project start scene {} does not exist", startScene.string());
	}
	
	void EditorLayer::SaveProject()
	{
		Ref<Project> project = Project::GetActive();
		if (project == nullptr)
			return;

		project->GetConfig().Renderer = Renderer::GetSettings();
		Project::SaveActive();
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
		Project::New(directory / "Sandbox.hyproj");
		HY_INFO("No project found in {}; started an unsaved one", directory.string());
	}

	void EditorLayer::SetActiveScene(const Ref<Scene>& scene)
	{
		m_Scene = scene;
		m_Scene->Init();
		m_SceneRenderer->SetScene(m_Scene);
		m_HierarchyWindow->ClearSelection();
	}

	void EditorLayer::NewScene()
	{
		SetActiveScene(CreateRef<Scene>("Untitled", true));
		CreateDefaultCamera();
		m_EditorScenePath.clear();
	}

	void EditorLayer::CreateDefaultCamera()
	{
		Entity camera = m_Scene->CreateEntity("Camera");
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

		SetActiveScene(newScene);
		m_EditorScenePath = path;
	}

	void EditorLayer::SaveScene()
	{
		// First save of a new scene goes to <project>/Assets/Scenes/<SceneName>.hyscene.
		if (m_EditorScenePath.empty())
			m_EditorScenePath = EditorPaths::GetScenesDirectory() / (EditorPaths::ToFileName(m_Scene->GetName()) + SceneSerializer::FileExtension);

		SceneSerializer serializer(m_Scene);
		serializer.Serialize(m_EditorScenePath);
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
		m_Scene->SetName(path.stem().string());

		SceneSerializer serializer(m_Scene);
		if (serializer.Serialize(path))
			m_EditorScenePath = path;
	}

	void EditorLayer::OnAssetDropped(const std::filesystem::path& path)
	{
		const std::string extension = path.extension().string();

		if (extension == PrefabSerializer::FileExtension)
		{
			if (Entity entity = PrefabSerializer::Instantiate(*m_Scene, path))
				m_HierarchyWindow->SetSelectedEntity(entity);
		}
		else if (extension == SceneSerializer::FileExtension)
		{
			OpenScene(path);
		}
	}
}
