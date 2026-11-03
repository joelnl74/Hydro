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
}
