#pragma once
#include <Hydro/Asset/AssetManager.h>

#include <imgui/imgui.h>

#include <cstdint>
#include <filesystem>

namespace Hydro
{
	// Drag & drop of assets between editor windows (project window -> viewport / hierarchy / inspector fields).
	// The payload is the asset's handle, so what is dropped is the asset itself (it stays the same asset
	// if it is renamed meanwhile); files that aren't registered assets carry nothing and can't be dropped.
	class AssetDragDrop
	{
	public:
		static constexpr const char* PayloadType = "HYDRO_ASSET_HANDLE";

		// Call between ImGui::BeginDragDropSource / EndDragDropSource.
		static void SetPayload(const std::filesystem::path& path)
		{
			const uint64_t handle = static_cast<uint64_t>(AssetManager::FindHandle(path));
			if (handle != 0)
			{
				ImGui::SetDragDropPayload(PayloadType, &handle, sizeof(handle));
			}
		}

		// Call between ImGui::BeginDragDropTarget / EndDragDropTarget. Returns true and fills `outHandle`
		// when an asset was dropped this frame. With a `type` other than None, only assets of that type
		// light up the target and can be dropped on it.
		static bool AcceptHandle(AssetHandle& outHandle, AssetType type = AssetType::None)
		{
			const ImGuiPayload* peek = ImGui::AcceptDragDropPayload(PayloadType, ImGuiDragDropFlags_AcceptPeekOnly);
			if (peek == nullptr || peek->DataSize != static_cast<int>(sizeof(uint64_t)))
			{
				return false;
			}

			const AssetHandle handle(*static_cast<const uint64_t*>(peek->Data));
			const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
			if (metadata == nullptr || metadata->IsMissing || (type != AssetType::None && metadata->Type != type))
			{
				return false;
			}

			// The right kind of asset: highlight the target, and deliver when the mouse is released.
			if (ImGui::AcceptDragDropPayload(PayloadType) == nullptr)
			{
				return false;
			}

			outHandle = handle;
			return true;
		}

		// Like AcceptHandle for any asset, as the file's absolute path.
		static bool AcceptPayload(std::filesystem::path& outPath)
		{
			AssetHandle handle = 0;
			if (AcceptHandle(handle) == false)
			{
				return false;
			}

			outPath = AssetManager::GetAbsolutePath(handle);
			return outPath.empty() == false;
		}
	};
}
