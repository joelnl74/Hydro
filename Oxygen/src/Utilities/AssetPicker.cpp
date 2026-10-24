#include "AssetPicker.h"
#include "AssetDragDrop.h"
#include "ThumbnailManager.h"
#include "../EditorWindows/PropertyUI.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Texture2D.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <vector>

namespace Hydro
{
	struct PickerRow
	{
		AssetHandle Handle = 0;
		std::string Name;   // file name
		std::string Folder; // folder inside the asset folder ("" = the root)
		std::string Lower;  // lower-case path, for the search
	};

	// Only one picker list is open at a time, so they share this.
	struct PickerState
	{
		ImGuiID Owner = 0;
		std::vector<PickerRow> Rows;
		std::vector<int> Visible;
		char Search[128] = {};
	};

	static PickerState s_State;

	static std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return text;
	}

	// The project's assets of `type` that exist, by path.
	static void BuildRows(AssetType type)
	{
		s_State.Rows.clear();
		for (const auto& entry : AssetManager::GetRegistry().GetAll())
		{
			const AssetMetadata& metadata = entry.second;
			if (metadata.Type != type || metadata.IsMissing)
			{
				continue;
			}

			PickerRow row;
			row.Handle = metadata.Handle;
			row.Name = metadata.Path.filename().u8string();
			row.Folder = metadata.Path.parent_path().generic_u8string();
			row.Lower = ToLower(metadata.Path.generic_u8string());
			s_State.Rows.push_back(std::move(row));
		}

		std::sort(s_State.Rows.begin(), s_State.Rows.end(), [](const PickerRow& a, const PickerRow& b) { return a.Lower < b.Lower; });
	}

	// The rows whose path contains the search text.
	static void FilterRows()
	{
		s_State.Visible.clear();
		const std::string search = ToLower(s_State.Search);
		for (size_t i = 0; i < s_State.Rows.size(); i++)
		{
			if (search.empty() || s_State.Rows[i].Lower.find(search) != std::string::npos)
			{
				s_State.Visible.push_back((int)i);
			}
		}
	}

	// The row's small image; an empty square while it is still being made.
	static void DrawRowThumbnail(AssetHandle handle, float size)
	{
		const Thumbnail& thumbnail = ThumbnailManager::Get(handle);
		if (thumbnail.Texture != nullptr)
		{
			PropertyUI::SpriteImage(thumbnail.Texture, 0, size);
		}
		else
		{
			ImGui::Dummy({ size, size });
		}
	}

	// The list popup of one field. Returns true when a row was chosen (`picked`).
	static bool DrawList(ImGuiID owner, AssetType type, AssetHandle current, AssetHandle& picked)
	{
		ImGui::SetNextWindowSize({ 380.0f, 360.0f }, ImGuiCond_Appearing);
		if (ImGui::BeginPopup("##AssetPicker") == false)
		{
			if (s_State.Owner == owner)
			{
				s_State.Owner = 0;
				s_State.Rows.clear();
				s_State.Visible.clear();
			}
			return false;
		}

		if (ImGui::IsWindowAppearing())
		{
			s_State.Owner = owner;
			s_State.Search[0] = '\0';
			BuildRows(type);
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Search", "Search", s_State.Search, sizeof(s_State.Search));
		FilterRows();
		ImGui::Separator();

		const bool showThumbnails = type == AssetType::Texture2D;
		const float rowHeight = showThumbnails ? 40.0f : ImGui::GetFrameHeight();
		bool chosen = false;

		if (ImGui::BeginChild("##Rows", { 0.0f, 0.0f }, false))
		{
			if (s_State.Visible.empty())
			{
				ImGui::TextDisabled(s_State.Rows.empty() ? "Nothing of this kind in the project yet." : "Nothing matches.");
			}

			ImGuiListClipper clipper;
			clipper.Begin((int)s_State.Visible.size(), rowHeight + ImGui::GetStyle().ItemSpacing.y);
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
					const PickerRow& row = s_State.Rows[(size_t)s_State.Visible[(size_t)i]];
					ImGui::PushID(i);

					// The row is one Selectable; its contents are drawn over it.
					const ImVec2 start = ImGui::GetCursorPos();
					if (ImGui::Selectable("##Row", row.Handle == current, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, rowHeight }))
					{
						picked = row.Handle;
						chosen = true;
					}
					const ImVec2 end = ImGui::GetCursorPos();

					ImGui::SetCursorPos(start);
					if (showThumbnails)
					{
						DrawRowThumbnail(row.Handle, rowHeight - 4.0f);
						ImGui::SameLine();
						ImGui::BeginGroup();
						ImGui::TextUnformatted(row.Name.c_str());
						ImGui::TextDisabled("%s", row.Folder.empty() ? "(asset folder)" : row.Folder.c_str());
						ImGui::EndGroup();
					}
					else
					{
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted(row.Name.c_str());
						ImGui::SameLine();
						ImGui::TextDisabled("%s", row.Folder.empty() ? "" : row.Folder.c_str());
					}
					ImGui::SetCursorPos(end);

					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();

		if (chosen)
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return chosen;
	}

	AssetPickerResult AssetPicker::DrawField(const char* id, AssetHandle current, AssetType type, const std::string& detail, float width)
	{
		AssetPickerResult result;
		ImGui::PushID(id);

		const float clearWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const bool hasValue = static_cast<uint64_t>(current) != 0;
		const AssetMetadata* metadata = AssetManager::GetMetadata(current);

		std::string label = "None";
		if (hasValue)
		{
			label = metadata == nullptr ? "(unknown asset)" : metadata->Path.filename().u8string() + (metadata->IsMissing ? " (missing)" : "");
		}

		if (ImGui::Button(label.c_str(), { (width > 0.0f ? width : ImGui::GetContentRegionAvail().x) - clearWidth - spacing, 0.0f }))
		{
			ImGui::OpenPopup("##AssetPicker");
		}

		if (ImGui::IsItemHovered())
		{
			if (metadata != nullptr)
			{
				ImGui::SetTooltip("%s%s%s\nClick to pick another, or drop one from the Project window.",
					metadata->Path.generic_u8string().c_str(), detail.empty() ? "" : "\n", detail.c_str());
			}
			else if (hasValue)
			{
				ImGui::SetTooltip("This asset isn't in the project (deleted, or from another project?).\nClick to pick another.");
			}
			else
			{
				ImGui::SetTooltip("Click to pick an asset, or drop one from the Project window.");
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			AssetHandle dropped = 0;
			if (AssetDragDrop::AcceptHandle(dropped, type))
			{
				result.Changed = true;
				result.Handle = dropped;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(hasValue == false);
		if (ImGui::Button("X", { clearWidth, 0.0f }))
		{
			result.Changed = true;
			result.Handle = 0;
		}
		ImGui::EndDisabled();

		AssetHandle picked = 0;
		if (DrawList(ImGui::GetID("##AssetPicker"), type, current, picked))
		{
			result.Changed = true;
			result.Handle = picked;
		}

		ImGui::PopID();
		return result;
	}
}
