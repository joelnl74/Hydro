#include "HierachyWindow.h"
#include "../EditorPaths.h"
#include "AssetDragDrop.h"

#include <imgui/imgui.h>
#include <Hydro/Core/UUID.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Utils/PlatformUtils.h>

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
		// Drop a selection that belongs to a scene that was replaced or an entity that was destroyed.
		if (m_SelectedEntity && (m_SelectedEntity.GetScene() != scene.get() || scene->m_Registry.valid(m_SelectedEntity) == false))
			m_SelectedEntity = {};

		// Destroying while iterating the registry is unsafe, so it happens after the loop.
		Entity entityToDelete;

		ImGui::Begin("Hierachy");
		ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen;
		const bool rootOpen = ImGui::TreeNodeEx("Root", flag);

		// Drop a prefab from the project window onto "Root" to instantiate it.
		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath) && droppedPath.extension().string() == PrefabSerializer::FileExtension)
			{
				if (Entity instance = PrefabSerializer::Instantiate(*scene, droppedPath))
					m_SelectedEntity = instance;
			}

			ImGui::EndDragDropTarget();
		}

		if (rootOpen)
		{
			scene->m_Registry.each([&](auto entityHandle)
			{
				if (scene->m_Registry.has<TagComponent>(entityHandle) == false || scene->m_Registry.has<IDComponent>(entityHandle) == false)
					return;

				Entity entity{ entityHandle, scene.get() };
				const std::string& tag = entity.GetName();
				const uint64_t id = entity.GetUUID();

				ImGuiTreeNodeFlags child = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (m_SelectedEntity == entity)
					child |= ImGuiTreeNodeFlags_Selected;

				bool opened = ImGui::TreeNodeEx((void*)id, child, "%s", tag.c_str());
				if (ImGui::IsItemClicked())
					m_SelectedEntity = entity;

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("Save as Prefab"))
						SaveAsPrefab(entity);
					if (ImGui::MenuItem("Delete Entity"))
						entityToDelete = entity;

					ImGui::EndPopup();
				}

				if (opened)
					ImGui::TreePop();
			});

			ImGui::TreePop();
		}

		// Right-click on empty space in the window.
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				m_SelectedEntity = scene->CreateEntity("Empty Entity");
				m_SelectedEntity.AddComponent<TransformComponent>();
			}

			if (ImGui::MenuItem("Create Sprite"))
			{
				m_SelectedEntity = scene->CreateEntity("Sprite");
				m_SelectedEntity.AddComponent<TransformComponent>();
				m_SelectedEntity.AddComponent<SpriteRendererComponent>(glm::vec4(0.9f, 0.4f, 0.2f, 1.0f));
			}

			if (ImGui::MenuItem("Create Camera"))
			{
				m_SelectedEntity = scene->CreateEntity("Camera");
				m_SelectedEntity.AddComponent<TransformComponent>();
				// Only the first camera in a scene is primary by default.
				m_SelectedEntity.AddComponent<CameraComponent>().Primary = (bool)scene->GetPrimaryCameraEntity() == false;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Instantiate Prefab..."))
			{
				std::string filepath = FileDialogs::OpenFile("Hydro Prefab (*.hyprefab)\0*.hyprefab\0");
				if (filepath.empty() == false)
				{
					if (Entity instance = PrefabSerializer::Instantiate(*scene, filepath))
						m_SelectedEntity = instance;
				}
			}

			ImGui::EndPopup();
		}

		ImGui::End();

		if (entityToDelete)
		{
			if (m_SelectedEntity == entityToDelete)
				m_SelectedEntity = {};

			scene->DestroyEntity(entityToDelete);
		}
	}

	void HierachyWindow::SaveAsPrefab(Entity entity)
	{
		PrefabSerializer::Save(entity, EditorPaths::GetPrefabPath(entity.GetName()));
	}
}
