#include "DataAssetInspector.h"
#include "InspectorUI.h"
#include "EditorWindows/DataFieldRenderer.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Data/Data.h>
#include <Hydro/Data/DataEditing.h>
#include <Hydro/Data/DataTableAsset.h>
#include <Hydro/Data/DataTypeRegistry.h>
#include <Hydro/Data/DefinitionAsset.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Hydro
{
	const DataReport& DataAssetInspector::GetValidation(AssetHandle handle)
	{
		const double now = ImGui::GetTime();
		if (m_ValidatedData != handle || m_ValidationTime < 0.0 || now - m_ValidationTime > 1.0)
		{
			m_Validation = Data::Validate(handle);
			m_ValidatedData = handle;
			m_ValidationTime = now;
		}
		return m_Validation;
	}

	void DataAssetInspector::MarkEdited(AssetHandle handle)
	{
		// Another file edited before this one is written now, so nothing waits for a save behind it.
		if ((uint64_t)m_UnsavedData != 0 && m_UnsavedData != handle)
		{
			Save(true);
		}
		m_UnsavedData = handle;
		m_ValidationTime = -1.0;
	}

	void DataAssetInspector::Save(bool force)
	{
		if ((uint64_t)m_UnsavedData == 0)
		{
			return;
		}
		// Not while a field is being typed in: a dialogue line would be written at every key.
		if (force == false && ImGui::IsAnyItemActive())
		{
			return;
		}

		const AssetHandle handle = m_UnsavedData;
		m_UnsavedData = 0;
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		const std::filesystem::path path = AssetManager::GetAbsolutePath(handle);
		if (metadata == nullptr || path.empty())
		{
			return;
		}

		bool saved = false;
		if (metadata->Type == AssetType::Definition)
		{
			const Ref<DefinitionAsset> definition = AssetManager::GetLoadedAsset<DefinitionAsset>(handle);
			saved = definition != nullptr && definition->Save(path);
		}
		else if (metadata->Type == AssetType::DataTable)
		{
			const Ref<DataTableAsset> table = AssetManager::GetLoadedAsset<DataTableAsset>(handle);
			saved = table != nullptr && table->Save(path);
		}
		if (saved)
		{
			// The object in memory is what the file holds now: it isn't loaded again under the inspector.
			AssetManager::MarkSaved(handle);
		}
	}

	// The header of a data asset: its file, what it holds and its validation line; a read-only file says why.
	static void DrawDataHeader(const std::filesystem::path& path, const char* kind, const DataTypeInfo* type, bool readOnly,
		const DataReport& report)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		if (type != nullptr)
		{
			ImGui::TextDisabled("%s of %s (%s)", kind, type->DisplayName.c_str(), type->Name.c_str());
		}
		DataFieldRenderer::DrawValidation(report);
		if (readOnly)
		{
			ImGui::TextColored({ 0.9f, 0.7f, 0.3f, 1.0f }, "Read-only: the file is newer than this build or its structure is broken (see above).\nFix it in a text editor; the editor won't overwrite it.");
		}
	}

	void DataAssetInspector::DrawDefinition(const std::filesystem::path& path)
	{
		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<DefinitionAsset> definition = Data::GetDefinitionAsset(handle);
		if (definition == nullptr)
		{
			DrawDataHeader(path, "Definition", nullptr, false, GetValidation(handle));
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the definition (see above and the console).");
			return;
		}

		DefinitionData& data = definition->GetData();
		DrawDataHeader(path, "Definition", definition->GetDataType(), data.ReadOnly, GetValidation(handle));
		ImGui::Separator();

		if (DataFieldRenderer::DrawFields(*data.Values.GetType(), data.Values.Get(), data.Values.GetKept(), data.ReadOnly))
		{
			MarkEdited(handle);
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Saved when an edit is finished; scripts see edits at once, also while playing.");
	}

	void DataAssetInspector::DrawTable(const std::filesystem::path& path)
	{
		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<DataTableAsset> table = Data::GetTableAsset(handle);
		if (table == nullptr)
		{
			DrawDataHeader(path, "Data table", nullptr, false, GetValidation(handle));
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the table (see above and the console).");
			return;
		}

		DataTableData& data = table->GetData();
		const bool readOnly = data.ReadOnly;
		DrawDataHeader(path, "Data table", table->GetRowType(), readOnly, GetValidation(handle));
		ImGui::Separator();

		// Another table: start on its first row.
		if (m_RowTable != handle)
		{
			m_RowTable = handle;
			m_SelectedRow = data.Rows.empty() ? std::string() : data.Rows.begin()->first;
			m_RowSearch[0] = '\0';
		}
		if (m_SelectedRow.empty() == false && data.Rows.find(m_SelectedRow) == data.Rows.end())
		{
			m_SelectedRow = data.Rows.empty() ? std::string() : data.Rows.begin()->first; // removed outside the editor
		}

		// The row actions.
		ImGui::BeginDisabled(readOnly);
		if (ImGui::Button("Add"))
		{
			m_SelectedRow = DataEditing::AddRow(data);
			MarkEdited(handle);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(m_SelectedRow.empty());
		if (ImGui::Button("Duplicate"))
		{
			m_SelectedRow = DataEditing::DuplicateRow(data, m_SelectedRow);
			MarkEdited(handle);
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete..."))
		{
			m_RowToDelete = m_SelectedRow;
			ImGui::OpenPopup("Delete Row?");
		}
		ImGui::SameLine();
		if (ImGui::Button("Change ID..."))
		{
			std::snprintf(m_RowIdBuffer, sizeof(m_RowIdBuffer), "%s", m_SelectedRow.c_str());
			ImGui::OpenPopup("Change Row ID");
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##RowSearch", "Search rows", m_RowSearch, sizeof(m_RowSearch));
		std::string search = m_RowSearch;
		std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return (char)std::tolower(c); });

		// The rows by ID: the display field's name, the ID under it.
		const float lineHeight = ImGui::GetTextLineHeight();
		const float rowHeight = lineHeight * 2.0f + 2.0f;
		const float listHeight = std::min(260.0f,
			std::max(1.0f, (float)data.Rows.size()) * (rowHeight + ImGui::GetStyle().ItemSpacing.y) + 8.0f);
		if (ImGui::BeginChild("##Rows", { 0.0f, listHeight }, true))
		{
			for (const auto& [id, record] : data.Rows)
			{
				const std::string label = DataEditing::GetRowLabel(record, id);
				if (search.empty() == false)
				{
					std::string lower = label + " " + id;
					std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
					if (lower.find(search) == std::string::npos)
					{
						continue;
					}
				}
				ImGui::PushID(id.c_str());
				const ImVec2 start = ImGui::GetCursorPos();
				if (ImGui::Selectable("##row", id == m_SelectedRow, ImGuiSelectableFlags_AllowItemOverlap, { 0.0f, rowHeight }))
				{
					m_SelectedRow = id;
				}
				const ImVec2 end = ImGui::GetCursorPos();
				ImGui::SetCursorPos({ start.x + 4.0f, start.y + 1.0f });
				ImGui::TextUnformatted(label.c_str());
				ImGui::SetCursorPos({ start.x + 4.0f, start.y + 1.0f + lineHeight });
				ImGui::TextDisabled("ID: %s", id.c_str());
				ImGui::SetCursorPos(end);
				ImGui::PopID();
			}
			if (data.Rows.empty())
			{
				ImGui::TextDisabled("No rows yet: Add makes one.");
			}
		}
		ImGui::EndChild();

		// Delete: references to the row (scenes, other rows, saves) will show it as missing.
		if (ImGui::BeginPopupModal("Delete Row?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete row '%s'?", m_RowToDelete.c_str());
			ImGui::TextDisabled("Scenes, other rows and saves that refer to it will show 'Missing: %s'.", m_RowToDelete.c_str());
			if (ImGui::Button("Delete", { 120.0f, 0.0f }))
			{
				DataEditing::RemoveRow(data, m_RowToDelete);
				m_SelectedRow = data.Rows.empty() ? std::string() : data.Rows.begin()->first;
				MarkEdited(handle);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Change ID: a row ID is its lasting identity, so this is a migration, not a rename.
		if (ImGui::BeginPopupModal("Change Row ID", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("New ID for '%s':", m_SelectedRow.c_str());
			if (ImGui::IsWindowAppearing())
			{
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(260.0f);
			const bool entered = ImGui::InputText("##RowId", m_RowIdBuffer, sizeof(m_RowIdBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
			const std::string problem = DataEditing::CheckRowId(data, m_SelectedRow, m_RowIdBuffer);
			if (problem.empty() == false)
			{
				ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "%s", problem.c_str());
			}
			ImGui::TextColored({ 0.9f, 0.7f, 0.3f, 1.0f }, "Scenes, other rows and saves refer to rows by ID: they will show\n'Missing: %s' until they are set to the new ID.\nThe display name is a field: edit it below instead.", m_SelectedRow.c_str());
			ImGui::BeginDisabled(problem.empty() == false);
			if (ImGui::Button("Change ID", { 120.0f, 0.0f }) || (entered && problem.empty()))
			{
				std::string ignored;
				const std::string newId = m_RowIdBuffer;
				if (DataEditing::ChangeRowId(data, m_SelectedRow, newId, ignored) && newId != m_SelectedRow)
				{
					m_SelectedRow = newId;
					MarkEdited(handle);
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// The selected row's values: the same renderer as a definition.
		auto selected = data.Rows.find(m_SelectedRow);
		if (selected != data.Rows.end())
		{
			ImGui::Spacing();
			ImGui::Text("%s", DataEditing::GetRowLabel(selected->second, selected->first).c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(ID: %s)", selected->first.c_str());
			ImGui::Separator();
			DataRecord& record = selected->second;
			if (DataFieldRenderer::DrawFields(*record.GetType(), record.Get(), record.GetKept(), readOnly))
			{
				MarkEdited(handle);
			}
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Saved when an edit is finished; scripts see edits at once, also while playing.");
	}
}
