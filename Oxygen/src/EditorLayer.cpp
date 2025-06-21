#include "EditorLayer.h"
// #include "../../Hydro/src/Hydro/Renderer/Renderer.h"
#include <iostream>

#include <imgui/imgui.h>
#include <string>

namespace Hydro
{
	void EditorLayer::OnAttach()
	{
		m_Scene = CreateRef<Scene>("Scene", false);
		m_SceneRenderer = new SceneRenderer(m_Scene);
		m_Scene->Init();
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
				if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {}
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

		ImGui::Begin("Hierachy");
		ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::TreeNodeEx("Root", flag))
		{
			ImGuiTreeNodeFlags child = ImGuiTreeNodeFlags_Leaf;
			bool opened1 = ImGui::TreeNodeEx("Square_Green", child, "Square_Green");
			if (opened1)
			{
				ImGui::TreePop();
			}

			bool opened2 = ImGui::TreeNodeEx("Square_Red", child, "Square_Red");
			if (opened2)
			{
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}
		ImGui::End();

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
}


