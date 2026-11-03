#include "ScriptFieldUI.h"
#include "InspectorUI.h"
#include "EditorWindows/DataFieldRenderer.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Scene/Scene.h>

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <climits>

namespace Hydro
{
	// The colour of a script field that needs attention: an empty Required one, or a reference whose entity is gone
	// or lacks what the field needs.
	static const ImVec4 s_ScriptWarningColor = { 1.0f, 0.45f, 0.35f, 1.0f };

	bool ScriptFieldUI::EntityRefWidget(Entity owner, const ScriptProperty& property, EntityRef& reference)
	{
		bool changed = false;
		Scene& scene = *owner.GetScene();
		const Entity target = reference.IsSet() ? scene.GetEntityByUUID(reference.GetId()) : Entity{};
		const bool fits = (bool)target == false || property.AcceptsEntity == nullptr || property.AcceptsEntity(target);
		const std::string needs = property.TargetType.empty() ? std::string("an entity") : "a " + property.TargetType;

		std::string preview = "(none)";
		std::string problem;
		if ((bool)target)
		{
			preview = target.GetName();
			if (fits == false)
			{
				preview += " (no " + property.TargetType + ")";
				problem = "This entity has no " + property.TargetType + ".";
			}
		}
		else if (reference.IsSet())
		{
			preview = "(missing)";
			problem = "The entity this referred to isn't in the scene any more.";
		}
		else if (property.Required)
		{
			problem = "Required: pick " + needs + ".";
		}

		if (problem.empty() == false)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, s_ScriptWarningColor);
		}
		const bool open = ImGui::BeginCombo("##value", preview.c_str());
		if (problem.empty() == false)
		{
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s%s%s", problem.c_str(), problem.empty() ? "" : "\n",
				("Needs " + needs + ". Pick one, or drag an entity from the hierarchy onto the field.").c_str());
		}
		if (open)
		{
			if (ImGui::Selectable("(none)", reference.IsSet() == false))
			{
				reference = EntityRef();
				changed = true;
			}
			for (const entt::entity handle : scene.GetEntityOrder())
			{
				const Entity other{ handle, &scene };
				if (other.HasComponent<IDComponent>() == false
					|| (property.AcceptsEntity != nullptr && property.AcceptsEntity(other) == false))
				{
					continue;
				}
				ImGui::PushID((int)(uint32_t)handle);
				if (ImGui::Selectable(other.GetName().c_str(), other == target))
				{
					reference = EntityRef(other.GetUUID());
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HYDRO_HIERARCHY_ENTITY"))
			{
				const Entity dropped{ *static_cast<const entt::entity*>(payload->Data), &scene };
				if (dropped.IsValid() && dropped.HasComponent<IDComponent>()
					&& (property.AcceptsEntity == nullptr || property.AcceptsEntity(dropped)))
				{
					reference = EntityRef(dropped.GetUUID());
					changed = true;
				}
				else
				{
					HY_WARN("{}: '{}' can't go there; it needs {}", property.DisplayName,
						dropped.IsValid() ? dropped.GetName() : std::string("(that entity)"), needs);
				}
			}
			ImGui::EndDragDropTarget();
		}
		return changed;
	}

	// The same with its label column.
	static bool DrawEntityRefField(const char* label, Entity owner, const ScriptProperty& property, EntityRef& reference)
	{
		bool changed = false;
		InspectorUI::Row(label, [&]() { changed = ScriptFieldUI::EntityRefWidget(owner, property, reference); });
		return changed;
	}

	// A list, enum or struct property drawn as a data field. The property's Range, colour, asset filter and row
	// type apply to the entries; entities in it are picked like a reference field.
	static bool DrawScriptDataField(const ScriptProperty& property, ScriptDataValue& data, Entity owner)
	{
		if (data.IsSet() == false)
		{
			ImGui::TextDisabled("%s: (no value)", property.DisplayName.c_str());
			return false;
		}
		DataField field;
		field.Name = property.Name;
		field.DisplayName = property.DisplayName;
		field.Type = *data.GetType();
		field.Tooltip = property.Tooltip;
		field.Min = property.Min;
		field.Max = property.Max;
		field.IsColor = property.IsColor;
		field.AssetFilter = property.AssetFilter;
		field.DataTypeFilter = property.DataTypeFilter;
		field.ReadOnly = property.ReadOnly;
		field.Access = [](void* object) -> void*
		{
			return object; // the value is the whole object
		};

		// A script value has no file record to keep unreadable or unknown parts in (ScriptSerializer reports them).
		DataKeptValues kept;
		DataFieldRenderer::SetEntityRefWidget([&owner, &property](EntityRef& reference)
		{
			return ScriptFieldUI::EntityRefWidget(owner, property, reference);
		});
		const bool changed = DataFieldRenderer::DrawField(field, data.Get(), kept, property.ReadOnly);
		DataFieldRenderer::SetEntityRefWidget(nullptr);
		return changed;
	}

	bool ScriptFieldUI::PropertyField(const ScriptProperty& property, ScriptPropertyValue& value, Entity owner)
	{
		bool changed = false;
		const char* label = property.DisplayName.c_str();

		ImGui::PushID(property.Name.c_str());
		ImGui::BeginDisabled(property.ReadOnly);

		switch (property.Type)
		{
			case ScriptPropertyType::Bool:
			{
				bool& current = std::get<bool>(value);
				InspectorUI::Row(label, [&]() { changed = ImGui::Checkbox("##value", &current); });
				break;
			}
			case ScriptPropertyType::Int:
			{
				int& current = std::get<int>(value);
				InspectorUI::Row(label, [&]()
				{
					changed = ImGui::InputInt("##value", &current);
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						const int clamped = (int)std::clamp((float)current, std::max(property.Min, (float)INT_MIN),
							std::min(property.Max, (float)INT_MAX));
						changed |= clamped != current;
						current = clamped;
					}
				});
				break;
			}
			case ScriptPropertyType::Float:
			{
				float& current = std::get<float>(value);
				InspectorUI::Row(label,
					[&]() { changed = InspectorUI::InputFloat("##value", current, "%.3f", property.Min, property.Max); });
				break;
			}
			case ScriptPropertyType::Vec2:
			{
				glm::vec2& current = std::get<glm::vec2>(value);
				changed = InspectorUI::DrawVector(label, glm::value_ptr(current), 2, 0.0f, property.Min, property.Max);
				break;
			}
			case ScriptPropertyType::Vec3:
			{
				glm::vec3& current = std::get<glm::vec3>(value);
				if (property.IsColor)
				{
					InspectorUI::Row(label, [&]() { changed = ImGui::ColorEdit3("##value", glm::value_ptr(current)); });
				}
				else
				{
					changed = InspectorUI::DrawVector(label, glm::value_ptr(current), 3, 0.0f, property.Min, property.Max);
				}
				break;
			}
			case ScriptPropertyType::Vec4:
			{
				glm::vec4& current = std::get<glm::vec4>(value);
				if (property.IsColor)
				{
					InspectorUI::Row(label, [&]() { changed = ImGui::ColorEdit4("##value", glm::value_ptr(current)); });
				}
				else
				{
					changed = InspectorUI::DrawVector(label, glm::value_ptr(current), 4, 0.0f, property.Min, property.Max);
				}
				break;
			}
			case ScriptPropertyType::String:
			{
				std::string& current = std::get<std::string>(value);
				InspectorUI::Row(label, [&]() { changed = InspectorUI::InputText("##value", current); });
				break;
			}
			case ScriptPropertyType::Asset:
			{
				AssetHandle& current = std::get<AssetHandle>(value);
				InspectorUI::Row(label, [&]()
				{
					// A definition or table property lists only files holding its data type.
					const AssetPickerResult result = property.DataTypeFilter.empty()
						? AssetPicker::DrawField("##value", current, property.AssetFilter)
						: AssetPicker::DrawDataField("##value", current, property.AssetFilter, property.DataTypeFilter);
					if (result.Changed)
					{
						current = result.Handle;
						changed = true;
					}
				});
				break;
			}
			case ScriptPropertyType::RowRef:
			{
				changed = DataFieldRenderer::DrawRowRef(label, std::get<DataRowRef>(value), property.DataTypeFilter);
				break;
			}
			case ScriptPropertyType::EntityRef:
			{
				changed = DrawEntityRefField(label, owner, property, std::get<EntityRef>(value));
				break;
			}
			case ScriptPropertyType::Data:
			{
				changed = DrawScriptDataField(property, std::get<ScriptDataValue>(value), owner);
				break;
			}
		}

		// An empty Required asset or row (references colour their own field).
		if (property.Required && property.Type != ScriptPropertyType::EntityRef)
		{
			const bool empty = property.Type == ScriptPropertyType::Asset ? (uint64_t)std::get<AssetHandle>(value) == 0
				: (property.Type == ScriptPropertyType::RowRef && std::get<DataRowRef>(value).IsSet() == false);
			if (empty)
			{
				ImGui::TextColored(s_ScriptWarningColor, "%s is required", label);
			}
		}

		ImGui::EndDisabled();
		if (property.Tooltip.empty() == false && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", property.Tooltip.c_str());
		}
		ImGui::PopID();
		return changed;
	}
}
