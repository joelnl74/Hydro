#include "ComponentDrawers.h"
#include "InspectorUI.h"
#include "ScriptFieldUI.h"
#include "EditorWindows/DataFieldRenderer.h"

#include <Hydro/Data/DataTypeRegistry.h>
#include <Hydro/Scene/ComponentRegistry.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Scene/Scripting/ScriptableEntity.h>

#include <string>

namespace Hydro
{
	// Scripts that are running (play mode) get OnDestroy before they go.
	static void RemoveAllScripts(Entity entity)
	{
		entity.GetScene()->RemoveScripts(entity);
	}

	void ComponentDrawers::DrawScripts(Entity entity, InspectorContext& context)
	{
		int scriptToRemove = -1;
		Section<NativeScriptComponent>("Scripts", entity, context, [&](NativeScriptComponent& scripts)
		{
			for (int i = 0; i < (int)scripts.Scripts.size(); i++)
			{
				NativeScriptComponent::Script& script = scripts.Scripts[i];
				ImGui::PushID(i);

				// Script name + remove button.
				ImGui::AlignTextToFramePadding();
				if (script.Name.empty())
				{
					ImGui::TextDisabled("(bound in code, not saved)");
				}
				else if (script.Instantiate == nullptr)
				{
					ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "%s (not registered)", script.Name.c_str());
				}
				else
				{
					ImGui::TextUnformatted(script.Name.c_str());
				}

				if (ImGui::IsItemHovered() && script.Instantiate == nullptr && script.Name.empty() == false)
				{
					ImGui::SetTooltip("No script is registered under this name. It is kept in the scene but doesn't run.");
				}

				ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
				if (ImGui::Button("X", { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
				{
					scriptToRemove = i;
				}

				// Exposed script fields are drawn directly under the script name.
				ScriptTypeInfo* typeInfo = ScriptRegistry::FindType(script.Name);
				if (typeInfo != nullptr && typeInfo->Properties.empty() == false)
				{
					ImGui::Spacing();

					// While the game runs, the fields show what the script holds now and edits go straight into it (#146).
					// They are made on the play copy, so Stop discards them like every other Play edit.
					ScriptableEntity* instance = script.Instance.get();
					for (const ScriptProperty& property : typeInfo->Properties)
					{
						if (property.HideInInspector)
						{
							continue;
						}

						// A property the binding has no value for yet, or whose type changed: start from the default.
						auto valueIt = script.Properties.find(property.Name);
						if (valueIt == script.Properties.end())
						{
							valueIt = script.Properties.emplace(property.Name, property.DefaultValue).first;
						}
						else if (ScriptPropertyHasType(valueIt->second, property.Type) == false
							|| (property.Type == ScriptPropertyType::Data
								&& std::get<ScriptDataValue>(valueIt->second).HasSameType(std::get<ScriptDataValue>(property.DefaultValue)) == false))
						{
							valueIt->second = property.DefaultValue;
						}

						ScriptPropertyValue& value = valueIt->second;
						if (instance != nullptr && property.Read != nullptr)
						{
							value = property.Read(*instance);
						}

						if (ScriptFieldUI::PropertyField(property, value, entity) && instance != nullptr && property.Apply != nullptr)
						{
							property.Apply(*instance, value);
						}
					}
					if (instance != nullptr)
					{
						ImGui::TextDisabled("Playing: edits apply now and are discarded on Stop.");
					}
				}

				// Saved values no property reads (renamed or removed, or the script isn't registered): kept so saving
				// loses nothing (#143). Drop them once they are known to be stale.
				if (script.KeptProperties.empty() == false)
				{
					std::string keys;
					for (const auto& entry : script.KeptProperties)
					{
						keys += (keys.empty() ? "" : ", ") + entry.first;
					}
					ImGui::AlignTextToFramePadding();
					ImGui::TextDisabled("Kept from the file: %s", keys.c_str());
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Saved values no property of this script reads now (it was renamed or removed, or the script isn't registered).\nThey are written back when the scene is saved. Give a renamed property a FormerName to read its value.");
					}
					if (script.Instantiate != nullptr)
					{
						ImGui::SameLine();
						if (ImGui::SmallButton("Drop"))
						{
							script.KeptProperties.clear();
						}
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("Forget these values: the next save leaves them out.");
						}
					}
				}

				if (i < (int)scripts.Scripts.size() - 1)
				{
					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();
				}

				ImGui::PopID();
			}

			ImGui::Spacing();
			if (ImGui::Button("Add Script", { ImGui::GetContentRegionAvail().x, 0.0f }))
			{
				ImGui::OpenPopup("AddScript");
			}

			if (ImGui::BeginPopup("AddScript"))
			{
				DrawScriptMenuItems(entity);
				ImGui::EndPopup();
			}
		}, &RemoveAllScripts);
		if (scriptToRemove >= 0)
		{
			context.Removals.push_back([scriptToRemove](Entity owner)
			{
				owner.GetScene()->RemoveScript(owner, (size_t)scriptToRemove);
			});
		}
	}

	void ComponentDrawers::DrawGameComponents(Entity entity, InspectorContext& context)
	{
		// What an EntityRef field accepts: any entity of the scene.
		static const ScriptProperty s_AnyEntity = []()
		{
			ScriptProperty property;
			property.DisplayName = "Entity";
			return property;
		}();

		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			void* data = type.Get(entity);
			if (data == nullptr)
			{
				continue;
			}
			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
			ImGui::PushID(type.Name.c_str());
			ImGui::Spacing();
			const bool open = ImGui::TreeNodeEx("##component", flags, "%s", type.DisplayName.c_str());
			if (ImGui::BeginPopupContextItem("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					const ComponentTypeInfo* removed = &type;
					context.Removals.push_back([removed](Entity owner) { removed->Remove(owner); });
				}
				ImGui::EndPopup();
			}
			if (open)
			{
				const DataTypeInfo* fields = DataTypeRegistry::Find(type.CppType);
				if (fields == nullptr)
				{
					ImGui::TextDisabled("Its fields aren't registered (run the reflection generator).");
				}
				else
				{
					DataKeptValues kept; // a component's unreadable values are logged when the scene loads
					DataFieldRenderer::SetEntityRefWidget([&entity](EntityRef& reference)
					{
						return ScriptFieldUI::EntityRefWidget(entity, s_AnyEntity, reference);
					});
					DataFieldRenderer::DrawFields(*fields, data, kept);
					DataFieldRenderer::SetEntityRefWidget(nullptr);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void ComponentDrawers::DrawGameMenuItems(Entity entity)
	{
		if (ComponentRegistry::GetTypes().empty() || ImGui::BeginMenu("Game") == false)
		{
			return;
		}
		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			if (ImGui::MenuItem(type.DisplayName.c_str(), nullptr, false, type.Has(entity) == false))
			{
				type.Add(entity);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", type.Name.c_str());
			}
		}
		ImGui::EndMenu();
	}

	void ComponentDrawers::DrawScriptMenuItems(Entity entity)
	{
		const std::vector<std::string> names = ScriptRegistry::GetNames();
		if (names.empty())
		{
			ImGui::TextDisabled("No scripts registered");
			ImGui::TextDisabled("Mark a ScriptableEntity class HY_SCRIPT() and build");
			return;
		}

		// A script runs once per entity, so ones already added are greyed out. The component is added when a script
		// is picked.
		const bool hasScripts = entity.HasComponent<NativeScriptComponent>();
		for (const std::string& name : names)
		{
			const bool added = hasScripts && entity.GetComponent<NativeScriptComponent>().Has(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, false, added == false))
			{
				if (hasScripts)
				{
					entity.GetComponent<NativeScriptComponent>().Bind(name);
				}
				else
				{
					entity.AddComponent<NativeScriptComponent>().Bind(name);
				}
				return;
			}
		}
	}
}
