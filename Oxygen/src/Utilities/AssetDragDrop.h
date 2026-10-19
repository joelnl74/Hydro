#pragma once
#include <imgui/imgui.h>

#include <filesystem>
#include <string>

namespace Hydro
{
	// Drag & drop of asset files between editor windows (project window -> viewport / hierarchy).
	// The payload is the file's absolute path as UTF-8.
	class AssetDragDrop
	{
	public:
		static constexpr const char* PayloadType = "HYDRO_ASSET_PATH";

		// Call between ImGui::BeginDragDropSource / EndDragDropSource.
		static void SetPayload(const std::filesystem::path& path)
		{
			const std::string utf8 = path.u8string();
			ImGui::SetDragDropPayload(PayloadType, utf8.c_str(), utf8.size() + 1);
		}

		// Call between ImGui::BeginDragDropTarget / EndDragDropTarget.
		// Returns true and fills `outPath` when an asset was dropped this frame.
		static bool AcceptPayload(std::filesystem::path& outPath)
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PayloadType);
			if (payload == nullptr)
				return false;

			outPath = std::filesystem::u8path(static_cast<const char*>(payload->Data));
			return true;
		}
	};
}
