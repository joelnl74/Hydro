#include "EditorContext.h"
#include "Utilities/AssetDragDrop.h"

#include <imgui/imgui.h>

namespace Hydro
{
	void EditorContext::AcceptAssetDrop()
	{
		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath) && OnAssetDropped)
			{
				OnAssetDropped(droppedPath);
			}

			ImGui::EndDragDropTarget();
		}
	}

	UUID EditorContext::GetSelectedUUID()
	{
		return (bool)Selection && Selection.IsValid() ? Selection.GetUUID() : UUID(0);
	}

	void EditorContext::Select(UUID id)
	{
		Selection = {};
		if ((uint64_t)id == 0 || ActiveScene == nullptr)
		{
			return;
		}
		Entity entity = ActiveScene->GetEntityByUUID(id);
		if ((bool)entity && entity.IsValid())
		{
			Selection = entity;
		}
	}

	SceneEditScope EditorContext::BeginSceneEdit(std::string name)
	{
		return SceneEditScope(Playing ? nullptr : EditorScene, std::move(name), GetSelectedUUID());
	}

	void EditorContext::FinishSceneEdit(SceneEditScope& edit)
	{
		Commands.Push(edit.Finish(GetSelectedUUID()));
	}

	void EditorContext::DeleteEntity(Entity entity)
	{
		if ((bool)entity == false || entity.IsValid() == false || ActiveScene == nullptr)
		{
			return;
		}
		SceneEditScope edit = BeginSceneEdit("Delete " + entity.GetName());
		edit.Capture(entity);
		// Its children go with it, so the selection may be one of them.
		if ((bool)Selection && (Selection == entity || ActiveScene->IsAncestorOf(entity, Selection)))
		{
			Selection = {};
		}
		ActiveScene->DestroyEntity(entity);
		FinishSceneEdit(edit);
	}

	int EditorContext::GetEntityKey()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (Playing || io.WantTextInput || (bool)Selection == false || Selection.IsValid() == false)
		{
			return 0;
		}
		if (io.KeyCtrl == false && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			return 1;
		}
		if (io.KeyCtrl && io.KeyShift == false && ImGui::IsKeyPressed(ImGuiKey_D, false))
		{
			return 2;
		}
		return 0;
	}

	Entity EditorContext::DuplicateEntity(Entity entity)
	{
		if ((bool)entity == false || entity.IsValid() == false || ActiveScene == nullptr)
		{
			return {};
		}
		SceneEditScope edit = BeginSceneEdit("Duplicate " + entity.GetName());
		Entity duplicate = ActiveScene->DuplicateEntity(entity);
		if (duplicate)
		{
			Selection = duplicate;
		}
		FinishSceneEdit(edit);
		return duplicate;
	}
}
