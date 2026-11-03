#include "DataFieldRenderer.h"
#include "PropertyUI.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Data/Data.h>
#include <Hydro/Data/DataEditing.h>
#include <Hydro/Data/DataTableAsset.h>
#include <Hydro/Data/DataTypeRegistry.h>

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <string>
#include <vector>

namespace Hydro
{
	// The colours the editor already uses for problems, readable on dark and light themes.
	static const ImVec4 s_ErrorColor = { 0.9f, 0.4f, 0.4f, 1.0f };
	static const ImVec4 s_WarningColor = { 0.9f, 0.7f, 0.3f, 1.0f };

	static std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return text;
	}

	static int StringResizeCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			std::string* text = static_cast<std::string*>(data->UserData);
			text->resize((size_t)data->BufTextLen);
			data->Buf = text->data();
		}
		return 0;
	}

	static bool InputText(const char* id, std::string& text)
	{
		return ImGui::InputText(id, text.data(), text.capacity() + 1, ImGuiInputTextFlags_CallbackResize, StringResizeCallback, &text);
	}

	// A value kept from the file, as one line ("Weapon", "[1, 2]").
	static std::string DumpNode(const YAML::Node& node)
	{
		if (node.IsScalar())
		{
			return node.Scalar();
		}
		YAML::Emitter out;
		out << YAML::Flow << node;
		return out.c_str();
	}

	// Clamps Int / Float / vector components into the field's Range once the field is left. Files are not clamped:
	// validation warns about them instead.
	static bool ClampOnLeave(const DataField& field, float* components, int count)
	{
		if (ImGui::IsItemDeactivatedAfterEdit() == false || field.HasRange() == false)
		{
			return false;
		}
		bool changed = false;
		for (int i = 0; i < count; i++)
		{
			const float clamped = std::clamp(components[i], field.Min, field.Max);
			changed |= clamped != components[i];
			components[i] = clamped;
		}
		return changed;
	}

	static bool DrawRecord(const DataTypeInfo& type, void* object, DataKeptValues& kept, bool readOnly);

	static DataFieldRenderer::EntityRefWidget s_EntityRefWidget;

	// The widget for a value of a "flat" kind (everything but Struct and List), filling the current item width.
	static bool DrawValueWidget(const DataField& field, const DataValueType& valueType, void* value)
	{
		switch (valueType.Kind)
		{
			case DataFieldKind::Bool:
				return ImGui::Checkbox("##value", static_cast<bool*>(value));
			case DataFieldKind::Int:
			{
				int& current = *static_cast<int*>(value);
				bool changed = ImGui::InputInt("##value", &current);
				if (ImGui::IsItemDeactivatedAfterEdit() && field.HasRange())
				{
					const int clamped = (int)std::clamp((float)current, std::max(field.Min, (float)INT_MIN),
						std::min(field.Max, (float)INT_MAX));
					changed |= clamped != current;
					current = clamped;
				}
				return changed;
			}
			case DataFieldKind::Float:
				return PropertyUI::InputFloat("##value", *static_cast<float*>(value), "%.3f", field.Min, field.Max);
			case DataFieldKind::Vec2:
			{
				float* components = glm::value_ptr(*static_cast<glm::vec2*>(value));
				const bool changed = ImGui::InputFloat2("##value", components, "%.3f");
				return ClampOnLeave(field, components, 2) || changed;
			}
			case DataFieldKind::Vec3:
			{
				float* components = glm::value_ptr(*static_cast<glm::vec3*>(value));
				if (field.IsColor)
				{
					return ImGui::ColorEdit3("##value", components);
				}
				const bool changed = ImGui::InputFloat3("##value", components, "%.3f");
				return ClampOnLeave(field, components, 3) || changed;
			}
			case DataFieldKind::Vec4:
			{
				float* components = glm::value_ptr(*static_cast<glm::vec4*>(value));
				if (field.IsColor)
				{
					return ImGui::ColorEdit4("##value", components);
				}
				const bool changed = ImGui::InputFloat4("##value", components, "%.3f");
				return ClampOnLeave(field, components, 4) || changed;
			}
			case DataFieldKind::String:
				return InputText("##value", *static_cast<std::string*>(value));
			case DataFieldKind::Asset:
			{
				AssetHandle& current = *static_cast<AssetHandle*>(value);
				const float width = ImGui::CalcItemWidth();
				const AssetPickerResult result = field.DataTypeFilter.empty()
					? AssetPicker::DrawField("##value", current, field.AssetFilter, {}, width)
					: AssetPicker::DrawDataField("##value", current, field.AssetFilter, field.DataTypeFilter, width);
				if (result.Changed)
				{
					current = result.Handle;
				}
				return result.Changed;
			}
			case DataFieldKind::Enum:
			{
				const DataEnumInfo* enumInfo = DataTypeRegistry::FindEnum(valueType.CppType);
				if (enumInfo == nullptr)
				{
					ImGui::TextColored(s_ErrorColor, "(enum not registered)");
					return false;
				}
				const int64_t number = valueType.ReadEnum(value);
				const char* name = enumInfo->FindName(number);
				const std::string preview = name != nullptr ? name : "(" + std::to_string(number) + ")";
				bool changed = false;
				if (ImGui::BeginCombo("##value", preview.c_str()))
				{
					for (const DataEnumValue& entry : enumInfo->Values)
					{
						if (ImGui::Selectable(entry.Name.c_str(), entry.Value == number))
						{
							valueType.WriteEnum(value, entry.Value);
							changed = true;
						}
					}
					ImGui::EndCombo();
				}
				return changed;
			}
			case DataFieldKind::RowRef:
				return DataFieldRenderer::DrawRowRefWidget(*static_cast<DataRowRef*>(value), field.DataTypeFilter, ImGui::CalcItemWidth());
			case DataFieldKind::EntityRef:
			{
				EntityRef& reference = *static_cast<EntityRef*>(value);
				if (s_EntityRefWidget != nullptr)
				{
					return s_EntityRefWidget(reference);
				}
				ImGui::TextDisabled("entity %llu", (unsigned long long)(uint64_t)reference.GetId());
				return false;
			}
			case DataFieldKind::Struct:
			case DataFieldKind::List:
				break;
		}
		return false;
	}

	// A one-line summary of a struct list entry for its header: its first non-empty text field, shortened.
	static std::string SummarizeStruct(const DataTypeInfo& type, const void* object)
	{
		for (const DataField& field : type.Fields)
		{
			if (field.Type.Kind == DataFieldKind::String)
			{
				const std::string& text = *static_cast<const std::string*>(field.Get(object));
				if (text.empty() == false)
				{
					std::string line = text.substr(0, text.find('\n'));
					return line.size() > 48 ? line.substr(0, 45) + "..." : line;
				}
			}
		}
		return type.DisplayName;
	}

	enum class ListAction : uint8_t
	{
		None,
		MoveUp,
		MoveDown,
		Remove
	};

	// The up / down / remove buttons of a list entry, right-aligned on the current line.
	static ListAction DrawEntryButtons(size_t index, size_t size)
	{
		ListAction action = ListAction::None;
		ImGui::BeginDisabled(index == 0);
		if (ImGui::SmallButton("^"))
		{
			action = ListAction::MoveUp;
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Move up");
		}
		ImGui::SameLine(0.0f, 2.0f);
		ImGui::BeginDisabled(index + 1 >= size);
		if (ImGui::SmallButton("v"))
		{
			action = ListAction::MoveDown;
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Move down");
		}
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::SmallButton("x"))
		{
			action = ListAction::Remove;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Remove");
		}
		return action;
	}

	static float EntryButtonsWidth()
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float button = ImGui::CalcTextSize("^").x + style.FramePadding.x * 2.0f;
		return button * 3.0f + 4.0f + style.ItemSpacing.x;
	}

	// A list field: a foldout with its entries, each with up, down and remove, and Add at the end. The kept values
	// of struct entries move with them.
	static bool DrawList(const char* label, const DataField& field, void* list, DataKeptValues& kept, bool readOnly)
	{
		const DataListOps& ops = *field.Type.List;
		const DataValueType& element = ops.Element;
		const size_t size = ops.Size(list);
		bool changed = false;

		const bool open = ImGui::TreeNodeEx("##list", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
			"%s  (%zu)", label, size);
		if (field.Tooltip.empty() == false && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", field.Tooltip.c_str());
		}
		if (open == false)
		{
			return false;
		}

		ListAction action = ListAction::None;
		size_t actionIndex = 0;
		const DataTypeInfo* structType = element.Kind == DataFieldKind::Struct ? DataTypeRegistry::Find(element.CppType) : nullptr;
		for (size_t i = 0; i < size; i++)
		{
			ImGui::PushID((int)i);
			void* entry = ops.At(list, i);
			ListAction entryAction = ListAction::None;
			if (structType != nullptr)
			{
				const bool entryOpen = ImGui::TreeNodeEx("##entry", ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth,
					"[%zu] %s", i, SummarizeStruct(*structType, entry).c_str());
				if (readOnly == false)
				{
					ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - EntryButtonsWidth()));
					entryAction = DrawEntryButtons(i, size);
				}
				if (entryOpen)
				{
					const std::string key = field.Name + "[" + std::to_string(i) + "]";
					changed |= DrawRecord(*structType, entry, kept.Nested[key], readOnly);
					ImGui::TreePop();
				}
			}
			else
			{
				const std::string entryLabel = "[" + std::to_string(i) + "]";
				PropertyUI::Row(entryLabel.c_str(), [&]()
				{
					ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - (readOnly ? 0.0f : EntryButtonsWidth())));
					ImGui::BeginDisabled(readOnly);
					if (DrawValueWidget(field, element, entry))
					{
						kept.ForgetUnreadable(field.Name);
						changed = true;
					}
					ImGui::EndDisabled();
					if (readOnly == false)
					{
						ImGui::SameLine();
						entryAction = DrawEntryButtons(i, size);
					}
				});
			}
			if (entryAction != ListAction::None)
			{
				action = entryAction;
				actionIndex = i;
			}
			ImGui::PopID();
		}

		if (readOnly == false && ImGui::SmallButton("+ Add"))
		{
			DataEditing::InsertListEntry(field.Type, list, kept, field.Name, size);
			changed = true;
		}

		// Changed after the loop, so no entry is drawn from a list that moved under it.
		switch (action)
		{
			case ListAction::MoveUp:
				DataEditing::SwapListEntries(field.Type, list, kept, field.Name, actionIndex, actionIndex - 1);
				changed = true;
				break;
			case ListAction::MoveDown:
				DataEditing::SwapListEntries(field.Type, list, kept, field.Name, actionIndex, actionIndex + 1);
				changed = true;
				break;
			case ListAction::Remove:
				DataEditing::RemoveListEntry(field.Type, list, kept, field.Name, actionIndex);
				changed = true;
				break;
			case ListAction::None:
				break;
		}

		ImGui::TreePop();
		return changed;
	}

	static bool DrawOneField(const DataField& field, void* object, DataKeptValues& kept, bool readOnly)
	{
		if (field.HideInInspector)
		{
			return false;
		}

		bool changed = false;
		const char* label = field.DisplayName.c_str();
		const bool fieldReadOnly = readOnly || field.ReadOnly;
		void* value = field.Get(object);
		ImGui::PushID(field.Name.c_str());

		// A value the file has that couldn't be read: shown above the field, which holds its default meanwhile.
		if (kept.Unreadable.IsMap() && kept.Unreadable[field.Name])
		{
			ImGui::TextColored(s_ErrorColor, "%s: the file has '%s', which can't be read. Setting the field replaces it.", label,
				DumpNode(kept.Unreadable[field.Name]).c_str());
		}

		switch (field.Type.Kind)
		{
			case DataFieldKind::Struct:
			{
				const DataTypeInfo* nestedType = DataTypeRegistry::Find(field.Type.CppType);
				if (nestedType == nullptr)
				{
					ImGui::TextColored(s_ErrorColor, "%s: its struct is not registered (HY_DATA?)", label);
					break;
				}
				if (ImGui::TreeNodeEx("##struct", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label))
				{
					changed = DrawRecord(*nestedType, value, kept.Nested[field.Name], fieldReadOnly);
					ImGui::TreePop();
				}
				break;
			}
			case DataFieldKind::List:
				changed = DrawList(label, field, value, kept, fieldReadOnly);
				break;
			default:
			{
				PropertyUI::Row(label, [&]()
				{
					ImGui::BeginDisabled(fieldReadOnly);
					changed = DrawValueWidget(field, field.Type, value);
					ImGui::EndDisabled();
				});
				if (field.Tooltip.empty() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("%s", field.Tooltip.c_str());
				}
				if (changed)
				{
					kept.Forget(field.Name);
				}
				break;
			}
		}

		ImGui::PopID();
		return changed;
	}

	static bool DrawRecord(const DataTypeInfo& type, void* object, DataKeptValues& kept, bool readOnly)
	{
		bool changed = false;
		for (const DataField& field : type.Fields)
		{
			changed |= DrawOneField(field, object, kept, readOnly);
		}

		// Keys the file has that the type doesn't (a field renamed or removed in C++): kept and written back.
		if (kept.Unknown.IsMap() && kept.Unknown.size() > 0)
		{
			std::string removed;
			ImGui::TextColored(s_WarningColor, "Kept from the file (%s has no such field now):", type.Name.c_str());
			for (const auto& entry : kept.Unknown)
			{
				const std::string key = entry.first.Scalar();
				ImGui::PushID(key.c_str());
				ImGui::BulletText("%s: %s", key.c_str(), DumpNode(entry.second).c_str());
				if (readOnly == false)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("Remove"))
					{
						removed = key;
					}
				}
				ImGui::PopID();
			}
			if (removed.empty() == false)
			{
				kept.Unknown.remove(removed);
				changed = true;
			}
		}
		return changed;
	}

	bool DataFieldRenderer::DrawFields(const DataTypeInfo& type, void* object, DataKeptValues& kept, bool readOnly)
	{
		return DrawRecord(type, object, kept, readOnly);
	}

	bool DataFieldRenderer::DrawField(const DataField& field, void* object, DataKeptValues& kept, bool readOnly)
	{
		return DrawOneField(field, object, kept, readOnly);
	}

	void DataFieldRenderer::SetEntityRefWidget(EntityRefWidget widget)
	{
		s_EntityRefWidget = std::move(widget);
	}

	bool DataFieldRenderer::DrawRowRefWidget(DataRowRef& ref, const std::string& rowType, float width)
	{
		bool changed = false;
		ImGui::PushID("##rowref");

		// The current choice: the row's name, or "Missing: <id>" when its table or the row is gone.
		const Ref<DataTableAsset> table = Data::GetTableAsset(ref.Table);
		const DataRecord* record = table != nullptr && ref.Row.empty() == false ? table->FindRow(ref.Row) : nullptr;
		const bool missing = ref.IsSet() && record == nullptr;
		std::string preview = "None";
		if (record != nullptr)
		{
			preview = DataEditing::GetRowLabel(*record, ref.Row);
		}
		else if (ref.Row.empty() == false || (uint64_t)ref.Table != 0)
		{
			preview = "Missing: " + (ref.Row.empty() ? std::string("(no row)") : ref.Row);
		}

		const float clearWidth = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(std::max(40.0f, (width > 0.0f ? width : ImGui::GetContentRegionAvail().x) - clearWidth - spacing));
		if (missing)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, s_WarningColor);
		}
		const bool open = ImGui::BeginCombo("##row", preview.c_str(), ImGuiComboFlags_HeightLarge);
		if (missing)
		{
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered() && open == false)
		{
			const AssetMetadata* metadata = AssetManager::GetMetadata(ref.Table);
			if (ref.IsSet())
			{
				ImGui::SetTooltip("%s : %s%s", metadata != nullptr ? metadata->Path.generic_u8string().c_str()
					: "(table not in the project)",
					ref.Row.c_str(), missing ? "\nThe row (or its table) is gone: pick another." : "");
			}
			else
			{
				ImGui::SetTooltip("Pick a row of a %s table", rowType.empty() ? "data" : rowType.c_str());
			}
		}

		if (open)
		{
			static char s_Search[64] = {};
			if (ImGui::IsWindowAppearing())
			{
				s_Search[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##search", "Search", s_Search, sizeof(s_Search));
			const std::string search = ToLower(s_Search);

			if (ImGui::Selectable("None", ref.IsSet() == false))
			{
				ref = DataRowRef{};
				changed = true;
			}

			// Every table of the row type, by path, its rows by ID.
			std::vector<const AssetMetadata*> tables;
			for (const auto& entry : AssetManager::GetRegistry().GetAll())
			{
				const AssetMetadata& metadata = entry.second;
				if (metadata.Type == AssetType::DataTable && metadata.IsMissing == false)
				{
					tables.push_back(&metadata);
				}
			}
			std::sort(tables.begin(), tables.end(), [](const AssetMetadata* a, const AssetMetadata* b) { return a->Path < b->Path; });

			size_t shown = 0;
			for (const AssetMetadata* metadata : tables)
			{
				const Ref<DataTableAsset> candidate = Data::GetTableAsset(metadata->Handle);
				if (candidate == nullptr || (rowType.empty() == false && candidate->GetRowType()->Name != rowType))
				{
					continue;
				}
				ImGui::Spacing();
				ImGui::TextDisabled("%s", metadata->Path.generic_u8string().c_str());
				ImGui::Separator();
				ImGui::PushID((int)(uint64_t)metadata->Handle);
				for (const auto& [id, row] : candidate->GetData().Rows)
				{
					const std::string rowLabel = DataEditing::GetRowLabel(row, id);
					if (search.empty() == false && ToLower(rowLabel).find(search) == std::string::npos
						&& ToLower(id).find(search) == std::string::npos)
					{
						continue;
					}
					shown++;
					const bool selected = ref.Table == metadata->Handle && ref.Row == id;
					if (ImGui::Selectable((rowLabel + "##" + id).c_str(), selected))
					{
						ref = DataRowRef{ metadata->Handle, id };
						changed = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("ID: %s", id.c_str());
					}
				}
				ImGui::PopID();
			}
			if (shown == 0 && search.empty())
			{
				ImGui::TextDisabled("No %s tables (or rows) in the project yet.", rowType.empty() ? "data" : rowType.c_str());
			}
			else if (shown == 0)
			{
				ImGui::TextDisabled("Nothing matches.");
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(ref.IsSet() == false && ref.Row.empty() && (uint64_t)ref.Table == 0);
		if (ImGui::Button("X", { clearWidth, 0.0f }))
		{
			ref = DataRowRef{};
			changed = true;
		}
		ImGui::EndDisabled();

		ImGui::PopID();
		return changed;
	}

	bool DataFieldRenderer::DrawRowRef(const char* label, DataRowRef& ref, const std::string& rowType)
	{
		bool changed = false;
		PropertyUI::Row(label, [&]() { changed = DrawRowRefWidget(ref, rowType); });
		return changed;
	}

	void DataFieldRenderer::DrawValidation(const DataReport& report)
	{
		const std::string summary = report.GetSummary();
		if (report.IsEmpty())
		{
			ImGui::TextDisabled("%s", summary.c_str());
			return;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, report.HasErrors() ? s_ErrorColor : s_WarningColor);
		const bool open = ImGui::TreeNodeEx("##validation", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", summary.c_str());
		ImGui::PopStyleColor();
		if (open)
		{
			for (const DataMessage& message : report.GetMessages())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, message.Severity == DataMessageSeverity::Error ? s_ErrorColor : s_WarningColor);
				ImGui::BulletText("%s", message.ToString().c_str());
				ImGui::PopStyleColor();
			}
			ImGui::TreePop();
		}
	}
}
