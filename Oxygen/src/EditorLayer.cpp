#include "EditorLayer.h"
#include <iostream>

#include <imgui/imgui.h>
#include <string>

#include "Hydro/Utils/PlatformUtils.h"
#include "EditorPaths.h"

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
		m_SceneRenderer = new SceneRenderer(m_Scene);
		m_Scene->Init();
		m_HierarchyWindow = CreateRef<HierachyWindow>();
		m_StatsWindow = CreateRef<StatsWindow>();

		CreateGalaxy();
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
			const float size = bright ? 4.0f + uniform(rng) * 3.0f : 1.5f + uniform(rng) * 2.0f;
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

		// Centre the galaxy in the viewport and fit it to the smaller side; squash Y a little
		// so it reads as a tilted disc.
		const glm::vec2 center = m_ViewportSize * 0.5f;
		const float scale = 0.47f * std::min(m_ViewportSize.x, m_ViewportSize.y);

		auto view = m_Scene->m_Registry.view<GalaxyStar, TransformComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			GalaxyStar& star = view.get<GalaxyStar>(entity);
			TransformComponent& transform = view.get<TransformComponent>(entity);

			star.Angle += star.AngularSpeed * deltaTime;

			const float r = star.Radius * scale;
			transform.Translation.x = center.x + std::cos(star.Angle) * r;
			transform.Translation.y = center.y + std::sin(star.Angle) * r * 0.78f;

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
	}

	void EditorLayer::OnUpdate()
	{
		auto now = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(now - m_LastFrameTime).count();
		m_LastFrameTime = now;
		deltaTime = std::min(deltaTime, 0.1f); // don't jump after a stall (e.g. resize / minimize)

		m_Scene->OnUpdate();
		UpdateGalaxy(deltaTime);
		m_SceneRenderer->RenderScene();
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
				if (ImGui::MenuItem("New Project...", "Ctrl+O"))
				{
					NewProject();
				}
				if (ImGui::MenuItem("Open Project...", "Ctrl+O")) 
				{
					OpenProject();
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
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_HierarchyWindow->OnImguiRender(m_Scene);
		m_StatsWindow->OnImguiRender(m_Scene);

		ImGui::Begin("Project");
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("ViewPort");
		ImVec2 viewPortSize = ImGui::GetContentRegionAvail();
		if (viewPortSize.x >= 1.0f && viewPortSize.y >= 1.0f)
			m_ViewportSize = { viewPortSize.x, viewPortSize.y };
		m_SceneRenderer->RenderCompositeImage(viewPortSize.x, viewPortSize.y);
		ImGui::End();
		ImGui::PopStyleVar();


		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e)
	{
	}
	
	void EditorLayer::NewProject()
	{
		std::string filepath = FileDialogs::OpenDirectory();

		if (filepath.empty())
			return;

		Project::New()->SetProjectDirectory(filepath);
		Project::Save(filepath);
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
		project->SetProjectDirectory(path.parent_path());
	}
	
	void EditorLayer::SaveProject()
	{
		Project::Save(Project::GetActive()->GetConfig().AssetDirectory);
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
		m_EditorScenePath.clear();
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
		if (!serializer.Deserialize(path))
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
}
