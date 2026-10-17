#include "HierachyWindow.h"

#include <imgui/imgui.h>
#include <Hydro/Core/UUID.h>

namespace Hydro
{
	HierachyWindow::HierachyWindow()
	{
	}
	HierachyWindow::~HierachyWindow()
	{
	}
	void HierachyWindow::OnImguiAttach()
	{
	}
	void HierachyWindow::OnImguiRender(Ref<Scene> scene)
	{
		ImGui::Begin("Hierachy");
		ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen;
		if (ImGui::TreeNodeEx("Root", flag))
		{
			scene->m_Registry.each([&](auto entity)
			{
				if (scene->m_Registry.has<TagComponent>(entity))
				{
					TagComponent& name = scene->m_Registry.get<TagComponent>(entity);
					IDComponent& id = scene->m_Registry.get<IDComponent>(entity);
					
					std::string x = std::to_string(id.Id);
					auto& tag = name.Tag + x;

					
					ImGuiTreeNodeFlags child = ImGuiTreeNodeFlags_Leaf;
					bool opened1 = ImGui::TreeNodeEx(tag.c_str(), child, tag.c_str());
					if (opened1)
					{
						ImGui::TreePop();
					}
				};
			});
		
		ImGui::TreePop();
		}
		ImGui::End();
	}

}
