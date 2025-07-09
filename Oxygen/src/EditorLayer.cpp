#include "EditorLayer.h"
#include <iostream>

#include <imgui/imgui.h>
#include <string>

#include "Hydro/Utils/PlatformUtils.h"

namespace Hydro
{
	void EditorLayer::OnAttach()
	{
		m_Scene = CreateRef<Scene>("Scene", false);
		m_SceneRenderer = new SceneRenderer(m_Scene);
		m_Scene->Init();
		m_HierarchyWindow = CreateRef<HierachyWindow>();

		Entity player = m_Scene->CreateEntity("Player Entity");
		player.AddComponent<TransformComponent>(glm::vec3(700, 450, 0), glm::vec3(128, 128, 0));
		player.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
		player.AddComponent<MovementComponent>().Bind<MovementComponent>(&player);
		player.AddComponent<PlayerComponent>().Bind<PlayerComponent>(&player);


		for (size_t i = 0; i < 24; i++)
		{
			for (size_t j = 0; j < 24; j++)
			{
				auto Sprite = m_Scene->CreateEntity("Entity");
				Sprite.AddComponent<TransformComponent>(glm::vec3(j * 64, i * 64, 0), glm::vec3(64, 64, 0));
				Sprite.AddComponent<SpriteRendererComponent>(glm::vec4(j * 0.05, i * 0.05, 1, 1));
			}
		}
	}

	void EditorLayer::OnDetach()
	{
	}

	void EditorLayer::OnUpdate()
	{
		m_Scene->OnUpdate();
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
				if (ImGui::MenuItem("New Scene", "Ctrl+N")) {}
				if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) 
				{
					Application::Get().Quit();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_HierarchyWindow->OnImguiRender(m_Scene);

		ImGui::Begin("Project");
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("ViewPort");
		ImVec2 viewPortSize = ImGui::GetContentRegionAvail();
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
		std::string filepath = FileDialogs::OpenFile("");

		if (filepath.empty())
			return;

		Project::New();
		Project::Save(filepath);
	}

	bool EditorLayer::OpenProject()
	{
		std::string filepath = FileDialogs::OpenFile("Hydro Project (*.hproj)\0*.hproj\0");
		
		if (filepath.empty())
			return false;

		OpenProject(filepath);
		
		return true;
	}
	
	void EditorLayer::OpenProject(const std::filesystem::path& path)
	{
	}
	
	void EditorLayer::SaveProject()
	{
		Project::Save(Project::GetActive()->GetConfig().AssetDirectory);
	}
}


