#include "TestLayer.h"
#include <iostream>

#include <imgui/imgui.h>

namespace Hydro
{
	TestLayer::TestLayer()
	{
	}

	TestLayer::~TestLayer()
	{
	}

	void TestLayer::OnAttach()
	{
	}

	void TestLayer::OnDetach()
	{
	}

	void TestLayer::OnUpdate()
	{
		if (Input::IsKeyPressed(KeyCode::A))
		{
			std::cout << "A" << std::endl;
		}
	}

	void TestLayer::OnImGuiRender()
	{
		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		ImGui::Begin("DockSpace Demo", false, window_flags);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Project...", "Ctrl+O"))

				ImGui::Separator();

				if (ImGui::MenuItem("New Scene", "Ctrl+N"))

				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))

				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
					// Application::Get().Close();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::End();
	}

	void TestLayer::OnEvent(Event& e)
	{
	}
}


