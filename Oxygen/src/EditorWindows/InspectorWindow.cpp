#include "InspectorWindow.h"
#include "../EditorPaths.h"
#include "Utilities/AssetPicker.h"
#include "PropertyUI.h"
#include "RenderLookUI.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Font.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/MaterialSerializer.h>
#include <Hydro/Renderer/Mesh.h>
#include <Hydro/Renderer/MeshImporter.h>
#include <Hydro/Renderer/MeshImportSerializer.h>
#include <Hydro/Renderer/RenderProfileSerializer.h>
#include <Hydro/Asset/AssetMetaSerializer.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Scene/SpriteAnimation.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>
#include <Hydro/Scene/SpriteAnimator.h>
#include <Hydro/Scene/Scripting/ScriptableEntity.h>
#include <Hydro/Scene/UI/UI.h>
#include <Hydro/Scene/UI/UILayout.h>
#include <Hydro/Scene/UI/UIScene.h>

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cstdio>
#include <string>

namespace Hydro
{
	constexpr float LabelColumnWidth = 100.0f;

	// A number you type into (click, type, Enter or click away). The value updates as you type;
	// it is clamped to [min, max] once you leave the field, so a half-typed number is never
	// clamped out from under you. Returns true when the value changed.
	static bool InputFloatField(const char* id, float& value, const char* format = "%.3f",
		float min = -FLT_MAX, float max = FLT_MAX)
	{
		bool changed = ImGui::InputFloat(id, &value, 0.0f, 0.0f, format);

		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			const float clamped = std::clamp(value, min, max);
			changed |= clamped != value;
			value = clamped;
		}

		return changed;
	}

	// One row: label on the left, X/Y(/Z/W) input fields on the right. Clicking an axis button
	// resets that axis. Each component is clamped to [min, max] when its field is left.
	// Returns true when a value changed.
	static bool DrawVectorControl(const char* label, float* values, int count, float resetValue = 0.0f,
		float min = -FLT_MAX, float max = FLT_MAX)
	{
		bool changed = false;

		ImGui::PushID(label);

		if (ImGui::BeginTable("##vector", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);

			ImGui::TableSetColumnIndex(1);

			const ImGuiStyle& style = ImGui::GetStyle();
			const float lineHeight = ImGui::GetFrameHeight();
			const ImVec2 buttonSize = { lineHeight, lineHeight };
			const float fieldWidth = (ImGui::GetContentRegionAvail().x - (float)count * buttonSize.x - (float)(count - 1) * style.ItemSpacing.x)
				/ (float)count;

			struct Axis { const char* Name; ImVec4 Color; ImVec4 Hovered; };
			const Axis axes[4] =
			{
				{ "X", { 0.80f, 0.10f, 0.15f, 1.0f }, { 0.90f, 0.20f, 0.20f, 1.0f } },
				{ "Y", { 0.20f, 0.70f, 0.20f, 1.0f }, { 0.30f, 0.80f, 0.30f, 1.0f } },
				{ "Z", { 0.10f, 0.25f, 0.80f, 1.0f }, { 0.20f, 0.35f, 0.90f, 1.0f } },
				{ "W", { 0.45f, 0.45f, 0.50f, 1.0f }, { 0.55f, 0.55f, 0.60f, 1.0f } },
			};

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, style.ItemSpacing.y });
			for (int i = 0; i < count; i++)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, axes[i].Color);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axes[i].Hovered);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, axes[i].Color);
				if (ImGui::Button(axes[i].Name, buttonSize))
				{
					values[i] = resetValue;
					changed = true;
				}
				ImGui::PopStyleColor(3);

				ImGui::SameLine();
				ImGui::SetNextItemWidth(fieldWidth);
				ImGui::PushID(i);
				changed |= InputFloatField("##value", values[i], "%.3f", min, max);
				ImGui::PopID();

				if (i < count - 1)
				{
					ImGui::SameLine(0.0f, style.ItemSpacing.x);
				}
			}
			ImGui::PopStyleVar();

			ImGui::EndTable();
		}

		ImGui::PopID();
		return changed;
	}

	static bool DrawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.0f)
	{
		return DrawVectorControl(label, glm::value_ptr(values), 3, resetValue);
	}

	// A label column + one widget, matching the Vec3 rows.
	template<typename WidgetFunction>
	static void DrawPropertyRow(const char* label, WidgetFunction widget)
	{
		ImGui::PushID(label);
		if (ImGui::BeginTable("##property", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			widget();

			ImGui::EndTable();
		}
		ImGui::PopID();
	}

	// ImGui::InputText on a std::string that grows as you type (what imgui_stdlib does).
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

	static bool InputTextField(const char* id, std::string& text)
	{
		return ImGui::InputText(id, text.data(), text.capacity() + 1, ImGuiInputTextFlags_CallbackResize, StringResizeCallback, &text);
	}

	// One HY_PROPERTY field of a script (#144, #145): the widget for its type, its range, colour picker, asset
	// filter, tooltip and read-only flag. `value` must hold the property's type. Returns true when it changed.
	static bool DrawScriptPropertyField(const ScriptProperty& property, ScriptPropertyValue& value)
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
				DrawPropertyRow(label, [&]() { changed = ImGui::Checkbox("##value", &current); });
				break;
			}
			case ScriptPropertyType::Int:
			{
				int& current = std::get<int>(value);
				DrawPropertyRow(label, [&]()
				{
					changed = ImGui::InputInt("##value", &current);
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						const int clamped = (int)std::clamp((float)current, std::max(property.Min, (float)INT_MIN), std::min(property.Max, (float)INT_MAX));
						changed |= clamped != current;
						current = clamped;
					}
				});
				break;
			}
			case ScriptPropertyType::Float:
			{
				float& current = std::get<float>(value);
				DrawPropertyRow(label, [&]() { changed = InputFloatField("##value", current, "%.3f", property.Min, property.Max); });
				break;
			}
			case ScriptPropertyType::Vec2:
			{
				glm::vec2& current = std::get<glm::vec2>(value);
				changed = DrawVectorControl(label, glm::value_ptr(current), 2, 0.0f, property.Min, property.Max);
				break;
			}
			case ScriptPropertyType::Vec3:
			{
				glm::vec3& current = std::get<glm::vec3>(value);
				if (property.IsColor)
				{
					DrawPropertyRow(label, [&]() { changed = ImGui::ColorEdit3("##value", glm::value_ptr(current)); });
				}
				else
				{
					changed = DrawVectorControl(label, glm::value_ptr(current), 3, 0.0f, property.Min, property.Max);
				}
				break;
			}
			case ScriptPropertyType::Vec4:
			{
				glm::vec4& current = std::get<glm::vec4>(value);
				if (property.IsColor)
				{
					DrawPropertyRow(label, [&]() { changed = ImGui::ColorEdit4("##value", glm::value_ptr(current)); });
				}
				else
				{
					changed = DrawVectorControl(label, glm::value_ptr(current), 4, 0.0f, property.Min, property.Max);
				}
				break;
			}
			case ScriptPropertyType::String:
			{
				std::string& current = std::get<std::string>(value);
				DrawPropertyRow(label, [&]() { changed = InputTextField("##value", current); });
				break;
			}
			case ScriptPropertyType::Asset:
			{
				AssetHandle& current = std::get<AssetHandle>(value);
				DrawPropertyRow(label, [&]()
				{
					const AssetPickerResult result = AssetPicker::DrawField("##value", current, property.AssetFilter);
					if (result.Changed)
					{
						current = result.Handle;
						changed = true;
					}
				});
				break;
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

	// A texture field (sprites, particle emitters): pick one of the project's images, or drop one from the
	// Project window; X clears it.
	static void DrawTextureField(const char* id, AssetHandle& textureHandle)
	{
		const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
		const std::string detail = texture != nullptr ? std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight()) + " px"
			: (static_cast<uint64_t>(textureHandle) != 0 ? "The texture is missing or couldn't be loaded (see the console)." : std::string());

		const AssetPickerResult result = AssetPicker::DrawField(id, textureHandle, AssetType::Texture2D, detail);
		if (result.Changed == false)
		{
			return;
		}

		// Loading the texture logs why a file couldn't be loaded; a handle that doesn't load is not taken.
		if (static_cast<uint64_t>(result.Handle) == 0 || AssetManager::GetAsset<Texture2D>(result.Handle) != nullptr)
		{
			textureHandle = result.Handle;
		}
	}

	// A font field (UI text): pick one of the project's .fnt fonts, or drop one from the Project window; X clears it.
	static void DrawFontField(const char* id, AssetHandle& fontHandle)
	{
		const Ref<Font> font = AssetManager::GetAsset<Font>(fontHandle);
		const std::string detail = font != nullptr ? font->GetData().Face + ", " + std::to_string((int)font->GetData().Size) + " px, " + std::to_string(font->GetData().Glyphs.size()) + " characters"
			: (static_cast<uint64_t>(fontHandle) != 0 ? "The font is missing or couldn't be loaded (see the console)." : std::string());

		const AssetPickerResult result = AssetPicker::DrawField(id, fontHandle, AssetType::Font, detail);
		if (result.Changed == false)
		{
			return;
		}
		if (static_cast<uint64_t>(result.Handle) == 0 || AssetManager::GetAsset<Font>(result.Handle) != nullptr)
		{
			fontHandle = result.Handle;
		}
	}

	// The Standard material's fields (a mesh slot's inline settings or a material asset). Returns true when
	// something changed.
	static bool DrawMaterialFields(MaterialData& material)
	{
		const MaterialData before = material;
		DrawPropertyRow("Albedo", [&]() { DrawTextureField("##MaterialAlbedo", material.Albedo); });
		DrawPropertyRow("Tint", [&]() { ImGui::ColorEdit4("##MaterialTint", glm::value_ptr(material.Tint)); });
		DrawPropertyRow("Tiling", [&]()
		{
			ImGui::DragFloat2("##MaterialTiling", glm::value_ptr(material.Tiling), 0.05f, 0.01f, 100.0f, "%.2f");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Texture repeats per metre.");
			}
		});
		DrawPropertyRow("Emissive", [&]() { DrawTextureField("##MaterialEmissive", material.Emissive); });
		DrawPropertyRow("Emission", [&]() { ImGui::ColorEdit3("##MaterialEmission", glm::value_ptr(material.EmissionColor)); });
		DrawPropertyRow("Intensity", [&]() { InputFloatField("##MaterialEmissionIntensity", material.EmissionIntensity, "%.2f", 0.0f, 100.0f); });
		DrawPropertyRow("Alpha cut-off", [&]()
		{
			InputFloatField("##MaterialAlphaCutoff", material.AlphaCutoff, "%.2f", 0.0f, 1.0f);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Pixels with less albedo alpha than this are dropped (railings, leaves). 0 = opaque.");
			}
		});
		DrawPropertyRow("Filter", [&]()
		{
			const char* filterNames[] = { "Nearest", "Linear" };
			int filter = material.Filter == TextureFilter::Linear ? 1 : 0;
			if (ImGui::Combo("##MaterialFilter", &filter, filterNames, 2))
			{
				material.Filter = filter == 1 ? TextureFilter::Linear : TextureFilter::Nearest;
			}
		});
		return material != before;
	}

	// "<folder>/<name>.hymat", or "<name> 2.hymat"... when taken.
	static std::filesystem::path MakeUniqueMaterialPath(const std::filesystem::path& folder, std::string name)
	{
		// Characters Windows doesn't allow in file names become '_'.
		for (char& c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
			{
				c = '_';
			}
		}
		if (name.empty())
		{
			name = "New Material";
		}

		std::error_code error;
		std::filesystem::path path = folder / std::filesystem::u8path(name + MaterialSerializer::FileExtension);
		for (int number = 2; std::filesystem::exists(path, error); number++)
		{
			path = folder / std::filesystem::u8path(name + " " + std::to_string(number) + MaterialSerializer::FileExtension);
		}
		return path;
	}

	// A FloatRange as two fields, Min and Max, clamped to [min, max] when left; Max never ends up below Min.
	static void DrawRangeField(const char* id, FloatRange& range, float min, float max, const char* format = "%.2f")
	{
		ImGui::PushID(id);
		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		ImGui::SetNextItemWidth(half);
		InputFloatField("##Min", range.Min, format, min, max);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Min: each particle picks a value between Min and Max.");
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		InputFloatField("##Max", range.Max, format, min, max);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Max");
		}
		if (ImGui::IsAnyItemActive() == false && range.Max < range.Min)
		{
			std::swap(range.Min, range.Max);
		}
		ImGui::PopID();
	}

	// A small heading between groups of rows in a long component section.
	static void DrawSubheading(const char* text)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", text);
		ImGui::Separator();
	}

	// For a sprite sheet (a Multiple texture): which of its sprites to draw.
	static void DrawSpriteField(uint32_t& spriteIndex, const Texture2D& texture)
	{
		const uint32_t current = spriteIndex < texture.GetSpriteCount() ? spriteIndex : 0;

		char preview[160];
		std::snprintf(preview, sizeof(preview), "%u: %s", current, texture.GetSprite(current).Name.c_str());
		if (ImGui::BeginCombo("##Sprite", preview, ImGuiComboFlags_HeightLarge))
		{
			for (uint32_t i = 0; i < texture.GetSpriteCount(); i++)
			{
				char label[160];
				std::snprintf(label, sizeof(label), "%u: %s", i, texture.GetSprite(i).Name.c_str());
				const bool selected = i == current;
				if (ImGui::Selectable(label, selected))
				{
					spriteIndex = i;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			const SpriteRegion& region = texture.GetSprite(current);
			ImGui::SetTooltip("%u x %u px at (%u, %u)\nPivot (%.2f, %.2f)\n%u sprites in this sheet",
				region.Size.x, region.Size.y, region.Position.x, region.Position.y, region.Pivot.x, region.Pivot.y, texture.GetSpriteCount());
		}
	}

	// The animator's .hyanim: pick one of the project's animation sets, or drop one from the Project window; X clears it.
	static void DrawAnimationSetField(Entity entity, SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> set = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		const std::string detail = set != nullptr ? std::to_string(set->GetClipCount()) + " clips"
			: (static_cast<uint64_t>(animator.Animations) != 0 ? "The animation set is missing or couldn't be loaded (see the console)." : std::string());

		const AssetPickerResult result = AssetPicker::DrawField("##Animations", animator.Animations, AssetType::SpriteAnimation, detail);
		if (result.Changed == false)
		{
			return;
		}

		// Loading the set logs why a file couldn't be loaded; a handle that doesn't load is not taken.
		if (static_cast<uint64_t>(result.Handle) == 0 || AssetManager::GetAsset<SpriteAnimationSet>(result.Handle) != nullptr)
		{
			animator.Animations = result.Handle;
			SpriteAnimator::ResetPlayback(entity);
		}
	}

	// The clip played on start: the set's default clip, or one picked by name.
	// Returns true when the choice changed.
	static bool DrawStartClipField(SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> setRef = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		const SpriteAnimationSet* set = setRef.get();
		const std::string defaultLabel = set != nullptr && set->GetClipCount() > 0
			? "Default (" + set->GetClip(set->GetDefaultClip()).Name + ")"
			: std::string("Default");

		bool changed = false;
		const char* preview = animator.StartClip.empty() ? defaultLabel.c_str() : animator.StartClip.c_str();
		ImGui::BeginDisabled(set == nullptr);
		if (ImGui::BeginCombo("##StartClip", preview, ImGuiComboFlags_HeightLarge))
		{
			if (ImGui::Selectable(defaultLabel.c_str(), animator.StartClip.empty()))
			{
				animator.StartClip.clear();
				changed = true;
			}
			for (uint32_t i = 0; set != nullptr && i < set->GetClipCount(); i++)
			{
				const std::string& name = set->GetClip(i).Name;
				if (ImGui::Selectable(name.c_str(), animator.StartClip == name))
				{
					animator.StartClip = name;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();

		if (set != nullptr && animator.StartClip.empty() == false && set->FindClip(animator.StartClip) < 0 && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("'%s' isn't in this set; the default clip plays instead.", animator.StartClip.c_str());
		}
		return changed;
	}

	// Shows the start clip's first frame on the entity's sprite, so the editor (where nothing
	// animates) shows the pose the game starts with.
	static void ShowStartFrame(Entity entity, const SpriteAnimatorComponent& animator)
	{
		const Ref<SpriteAnimationSet> setRef = AssetManager::GetAsset<SpriteAnimationSet>(animator.Animations);
		if (setRef == nullptr || setRef->GetClipCount() == 0 || entity.HasComponent<SpriteRendererComponent>() == false)
		{
			return;
		}

		const SpriteAnimationSet& set = *setRef;
		const int32_t clipIndex = set.FindStartClip(animator.StartClip);

		const SpriteAnimationClip& clip = set.GetClip((uint32_t)clipIndex);
		if (clip.Frames.empty())
		{
			return;
		}

		SpriteRendererComponent& sprite = entity.GetComponent<SpriteRendererComponent>();
		sprite.Texture = set.GetTexture() != nullptr ? set.GetTexture()->GetHandle() : AssetHandle(0);
		sprite.SpriteIndex = clip.Frames[0].Sprite;
		sprite.FlipX = clip.FlipX;
	}

	// The rows both 2D colliders share: how the surface behaves, and whether it is only a trigger.
	static void DrawColliderRows(PhysicsMaterial2D& material, bool& isSensor)
	{
		DrawPropertyRow("Density", [&]() { InputFloatField("##Density", material.Density, "%.2f", 0.0f, 100000.0f); });
		DrawPropertyRow("Friction", [&]() { InputFloatField("##Friction", material.Friction, "%.2f", 0.0f, 100.0f); });
		DrawPropertyRow("Restitution", [&]() { InputFloatField("##Restitution", material.Restitution, "%.2f", 0.0f, 1.0f); });
		DrawPropertyRow("Is Sensor", [&]()
		{
			ImGui::Checkbox("##IsSensor", &isSensor);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A sensor detects overlaps (trigger events) but nothing bounces off it.");
			}
		});
	}

	// Which layer a 3D collider or character is on (PhysicsLayer).
	static void DrawPhysicsLayerRow(PhysicsLayer& layer)
	{
		const char* names[] = { "World", "Player", "Npc", "Trigger" };
		int index = (int)layer;
		DrawPropertyRow("Layer", [&]()
		{
			if (ImGui::Combo("##PhysicsLayer", &index, names, 4))
			{
				layer = (PhysicsLayer)index;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("World: ground, walls, props. Player / Npc: characters and what walks with them.\nTrigger: blocks nothing; found by queries.");
			}
		});
	}

	// The rows every 3D collider shares: its layer, and whether it only detects characters walking in and out.
	static void DrawCollider3DRows(Entity entity, PhysicsLayer& layer, bool& isTrigger)
	{
		DrawPhysicsLayerRow(layer);
		DrawPropertyRow("Is Trigger", [&]()
		{
			ImGui::Checkbox("##IsTrigger", &isTrigger);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A trigger blocks nothing: characters walking in and out send OnTriggerEnter / OnTriggerExit.");
			}
		});
		ImGui::TextDisabled(entity.HasComponent<NativeScriptComponent>() ? "Scripted: the collider follows the transform while playing."
			: "A static collider. Offset is its centre, before the transform's scale.");
	}

	// Collapsible section for one component type. Right-click the header to remove it.
	// Removal is only *requested* here; the caller applies it after drawing, so no
	// component reference is held while EnTT storage changes.
	template<typename T, typename UIFunction>
	static void DrawComponent(const char* name, Entity entity, bool& removeRequested, UIFunction uiFunction)
	{
		if (entity.HasComponent<T>() == false)
			return;

		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

		ImGui::PushID(name);
		ImGui::Spacing();
		bool open = ImGui::TreeNodeEx("##component", flags, "%s", name);

		if (ImGui::BeginPopupContextItem("ComponentSettings"))
		{
			if (ImGui::MenuItem("Remove Component"))
				removeRequested = true;

			ImGui::EndPopup();
		}

		if (open)
		{
			uiFunction(entity.GetComponent<T>());
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void InspectorWindow::OnImguiRender(Entity entity, const std::filesystem::path& asset)
	{
		ImGui::Begin("Inspector");

		const bool hasEntity = entity && entity.GetScene() != nullptr && entity.GetScene()->m_Registry.valid(entity);
		if (hasEntity == false)
		{
			if (asset.empty())
			{
				ImGui::TextDisabled("Select an entity in the hierarchy, or an asset in the Project window.");
			}
			else
			{
				DrawAsset(asset);
			}
			SaveEditedMaterial(false);
			RenderLookUI::SaveEditedProfile(false);
			ImGui::End();
			return;
		}

		DrawEntityHeader(entity);
		DrawComponents(entity);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		DrawAddComponentButton(entity);
		DrawPrefabButton(entity);

		SaveEditedMaterial(false);
		RenderLookUI::SaveEditedProfile(false);
		ImGui::End();
	}

	void InspectorWindow::EditMaterialAsset(const Ref<MaterialAsset>& asset)
	{
		MaterialData data = asset->GetData();
		if (DrawMaterialFields(data))
		{
			// Every mesh using it changes now; the file is written once the edit is finished.
			if (m_UnsavedMaterial != asset->GetHandle())
			{
				SaveEditedMaterial(true);
			}
			asset->SetData(data);
			m_UnsavedMaterial = asset->GetHandle();
		}
	}

	void InspectorWindow::SaveEditedMaterial(bool force)
	{
		if ((uint64_t)m_UnsavedMaterial == 0)
		{
			return;
		}
		// Not while a field is being dragged or typed in: a slider would write the file every frame.
		if (force == false && ImGui::IsAnyItemActive())
		{
			return;
		}

		const AssetHandle handle = m_UnsavedMaterial;
		m_UnsavedMaterial = 0;
		const Ref<MaterialAsset> asset = AssetManager::GetLoadedAsset<MaterialAsset>(handle);
		const std::filesystem::path path = AssetManager::GetAbsolutePath(handle);
		if (asset == nullptr || path.empty())
		{
			return;
		}
		MaterialSerializer::Serialize(path, asset->GetData());
	}

	void InspectorWindow::AssignModelMaterials(MeshComponent& mesh, const Mesh& model)
	{
		// One slot per model material, each using the material asset the import made for it.
		const uint32_t count = model.GetMaterialSlotCount();
		mesh.Materials.assign(count, MeshMaterialSlot{});
		const std::vector<MeshMaterialInfo>& infos = model.GetMaterials();
		for (uint32_t slot = 0; slot < count && slot < (uint32_t)infos.size(); slot++)
		{
			const AssetHandle handle = infos[slot].DefaultMaterial;
			const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetAsset<MaterialAsset>(handle) : nullptr;
			if (asset != nullptr)
			{
				mesh.Materials[slot].Material = handle;
				mesh.Materials[slot].Inline = asset->GetData();
			}
		}
	}

	void InspectorWindow::DrawMeshMaterialSlots(Entity entity, MeshComponent& mesh, const Mesh* model)
	{
		// A built-in shape has one slot: shown open. A model's slots are listed by the model's material names.
		if (model == nullptr && mesh.Materials.size() == 1)
		{
			ImGui::Separator();
			ImGui::TextUnformatted("Material");
			DrawMeshMaterialSlot(entity, mesh.Materials[0]);
			return;
		}

		ImGui::Separator();
		ImGui::Text("Materials (%u)", (uint32_t)mesh.Materials.size());
		for (uint32_t i = 0; i < (uint32_t)mesh.Materials.size(); i++)
		{
			std::string name = "Slot " + std::to_string(i);
			if (model != nullptr && i < model->GetMaterials().size() && model->GetMaterials()[i].Name.empty() == false)
			{
				name += ": " + model->GetMaterials()[i].Name;
			}
			ImGui::PushID((int)i);
			if (ImGui::TreeNodeEx(name.c_str(), mesh.Materials.size() <= 2 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
			{
				DrawMeshMaterialSlot(entity, mesh.Materials[i]);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	void InspectorWindow::DrawMeshMaterialSlot(Entity entity, MeshMaterialSlot& slot)
	{
		const Ref<MaterialAsset> asset = (uint64_t)slot.Material != 0 ? AssetManager::GetAsset<MaterialAsset>(slot.Material) : nullptr;

		DrawPropertyRow("Asset", [&]()
		{
			const std::string detail = (uint64_t)slot.Material != 0 && asset == nullptr
				? "The material is missing or couldn't be loaded (see the console); the settings below are drawn." : std::string();
			const AssetPickerResult result = AssetPicker::DrawField("##MeshMaterialAsset", slot.Material, AssetType::Material, detail);
			if (result.Changed == false)
			{
				return;
			}

			if ((uint64_t)result.Handle == 0)
			{
				// Detached: the slot keeps looking the same, now with its own copy of the settings.
				if (asset != nullptr)
				{
					slot.Inline = asset->GetData();
				}
				slot.Material = 0;
				return;
			}

			// Loading logs why a file couldn't be loaded; a handle that doesn't load is not taken.
			const Ref<MaterialAsset> picked = AssetManager::GetAsset<MaterialAsset>(result.Handle);
			if (picked != nullptr)
			{
				slot.Material = result.Handle;
				// The fallback, should the file go missing later.
				slot.Inline = picked->GetData();
			}
		});

		if (asset != nullptr)
		{
			const std::filesystem::path path = AssetManager::GetAbsolutePath(slot.Material);
			ImGui::TextDisabled("Shared: edits change every mesh using %s.", path.filename().u8string().c_str());
			EditMaterialAsset(asset);
			return;
		}

		if ((uint64_t)slot.Material != 0)
		{
			ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Material asset missing: drawing these settings.");
		}
		DrawMaterialFields(slot.Inline);

		// Turns these settings into a material asset other meshes can share.
		if ((uint64_t)slot.Material == 0 && AssetManager::IsProjectOpen() && ImGui::Button("Save as Material Asset"))
		{
			const std::filesystem::path folder = AssetManager::GetAssetDirectory() / "Materials";
			std::error_code error;
			std::filesystem::create_directories(folder, error);
			const std::string name = entity.HasComponent<TagComponent>() ? entity.GetComponent<TagComponent>().Tag : std::string();
			const std::filesystem::path path = MakeUniqueMaterialPath(folder, name);
			if (MaterialSerializer::Serialize(path, slot.Inline))
			{
				const AssetHandle handle = AssetManager::FindHandle(path);
				if ((uint64_t)handle != 0 && AssetManager::GetAsset<MaterialAsset>(handle) != nullptr)
				{
					slot.Material = handle;
					HY_INFO("Created {}", path.filename().u8string());
				}
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Writes these settings to Assets/Materials as a .hymat and uses it here; pick it on other meshes to share it.");
		}
	}

	void InspectorWindow::DrawModelAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Model (glTF)");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const std::filesystem::path sidecar = AssetMetaSerializer::GetSidecarPath(path);
		MeshImportMapping mapping;
		if (path != m_ModelSettingsPath)
		{
			m_ModelSettingsPath = path;
			MeshImportSerializer::Deserialize(sidecar, m_ModelSettings, mapping);
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Import settings");
		DrawPropertyRow("Scale", [&]() { InputFloatField("##ModelScale", m_ModelSettings.Scale, "%.3f", 0.001f, 1000.0f); });
		DrawPropertyRow("Y up to Z up", [&]()
		{
			ImGui::Checkbox("##ModelYUp", &m_ModelSettings.ConvertYUpToZUp);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("glTF is Y-up; the engine is Z-up. On: the model stands up and its front (+Z) faces -Y, towards the diorama camera.");
			}
		});
		DrawPropertyRow("Make normals", [&]() { ImGui::Checkbox("##ModelNormals", &m_ModelSettings.GenerateNormals); });
		if (ImGui::Button("Apply (re-import)") && (uint64_t)handle != 0)
		{
			// Keeps the mapping, so its textures and materials keep their handles.
			MeshImportConfig unused;
			MeshImportSerializer::Deserialize(sidecar, unused, mapping);
			MeshImportSerializer::Serialize(sidecar, m_ModelSettings, mapping);
			AssetManager::ReloadAsset<Mesh>(handle);
		}

		const Ref<Mesh> mesh = (uint64_t)handle != 0 ? AssetManager::GetAsset<Mesh>(handle) : nullptr;
		ImGui::Separator();
		if (mesh == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not import the model (see the console).");
			return;
		}

		const glm::vec3 size = mesh->GetBoundsMax() - mesh->GetBoundsMin();
		ImGui::Text("%u sub-meshes, %u triangles, %u vertices", (uint32_t)mesh->GetSubMeshes().size(), mesh->GetIndexCount() / 3, mesh->GetVertexCount());
		ImGui::Text("Size %.2f x %.2f x %.2f m (bottom at z %.2f)", size.x, size.y, size.z, mesh->GetBoundsMin().z);
		if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint32_t i = 0; i < (uint32_t)mesh->GetMaterials().size(); i++)
			{
				const MeshMaterialInfo& info = mesh->GetMaterials()[i];
				const std::filesystem::path file = (uint64_t)info.DefaultMaterial != 0 ? AssetManager::GetAbsolutePath(info.DefaultMaterial) : std::filesystem::path();
				ImGui::BulletText("%u %s: %s", i, info.Name.c_str(), file.empty() ? "(none)" : file.filename().u8string().c_str());
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Sub-meshes"))
		{
			for (const SubMesh& subMesh : mesh->GetSubMeshes())
			{
				ImGui::BulletText("%s (primitive %u): %u triangles, slot %u", subMesh.SourceName.c_str(), subMesh.SourcePrimitive, subMesh.IndexCount / 3, subMesh.MaterialSlot);
			}
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::TextDisabled("Drag it into the viewport to place it, or pick it in a Mesh component's Model field.");
	}

	void InspectorWindow::DrawMaterialAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Material (Standard)");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<MaterialAsset> asset = (uint64_t)handle != 0 ? AssetManager::GetAsset<MaterialAsset>(handle) : nullptr;
		if (asset == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the material (see the console).");
			return;
		}

		ImGui::Separator();
		EditMaterialAsset(asset);
		ImGui::Spacing();
		ImGui::TextDisabled("Edits change every mesh that uses this material; saved when an edit is finished.");
	}

	void InspectorWindow::DrawRenderProfileAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		ImGui::TextDisabled("Render profile");

		const AssetHandle handle = AssetManager::FindHandle(path);
		const Ref<RenderProfile> profile = (uint64_t)handle != 0 ? AssetManager::GetAsset<RenderProfile>(handle) : nullptr;
		if (profile == nullptr)
		{
			ImGui::TextColored({ 0.9f, 0.4f, 0.4f, 1.0f }, "Could not read the render profile (see the console).");
			return;
		}

		ImGui::Separator();
		RenderLookUI::EditProfile(profile);
		ImGui::Spacing();
		ImGui::TextDisabled("Edits change every scene that uses this profile; saved when an edit is finished.\nA scene uses it from Render Settings > Look (scene).");
	}

	void InspectorWindow::DrawEntityHeader(Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			std::string& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", tag.c_str());

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
				tag = buffer;
		}

		if (entity.HasComponent<IDComponent>())
			ImGui::TextDisabled("UUID %llu", (unsigned long long)(uint64_t)entity.GetComponent<IDComponent>().Id);
	}

	// Physics bodies are made from the world transform, but under a rotated parent with non-uniform scale a shape
	// would need shear to match what is drawn, which it can't have.
	static bool HasNonUniformlyScaledParent(Entity entity)
	{
		Scene* scene = entity.GetScene();
		for (Entity parent = scene->GetParent(entity); (bool)parent; parent = scene->GetParent(parent))
		{
			if (parent.HasComponent<TransformComponent>())
			{
				const glm::vec3 scale = glm::abs(parent.GetComponent<TransformComponent>().Scale);
				if (std::abs(scale.x - scale.y) > 1e-4f || std::abs(scale.y - scale.z) > 1e-4f)
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool HasPhysicsBody(Entity entity)
	{
		return entity.HasComponent<Rigidbody2DComponent>() || entity.HasComponent<BoxCollider2DComponent>()
			|| entity.HasComponent<CircleCollider2DComponent>() || entity.HasComponent<BoxColliderComponent>()
			|| entity.HasComponent<SphereColliderComponent>() || entity.HasComponent<CapsuleColliderComponent>()
			|| entity.HasComponent<CylinderColliderComponent>() || entity.HasComponent<MeshColliderComponent>()
			|| entity.HasComponent<CharacterBodyComponent>();
	}

	void InspectorWindow::DrawComponents(Entity entity)
	{
		bool removeTransform = false;
		bool removeSprite = false;
		bool removeCamera = false;
		bool removeAnimator = false;
		bool removeLight = false;
		bool removeShadow = false;
		bool removeMesh = false;
		bool removeDirectionalLight = false;
		bool removeRigidbody = false;
		bool removeBoxCollider = false;
		bool removeCircleCollider = false;
		bool removeBoxCollider3D = false;
		bool removeSphereCollider = false;
		bool removeCapsuleCollider = false;
		bool removeCylinderCollider = false;
		bool removeMeshCollider = false;
		bool removeCharacterBody = false;
		bool removeEmitter = false;
		bool removeUICanvas = false;
		bool removeUIRect = false;
		bool removeUIImage = false;
		bool removeUIText = false;
		bool removeUIButton = false;
		bool removeUILayoutGroup = false;
		bool removeScripts = false;
		bool makePrimary = false;

		// A UI element is placed by its rect; a transform it may still carry is ignored, so it isn't shown.
		const bool isUIElement = entity.HasComponent<UIRectComponent>();
		if (isUIElement == false)
		{
			DrawComponent<TransformComponent>("Transform", entity, removeTransform, [&](TransformComponent& transform)
			{
				DrawVec3Control("Translation", transform.Translation);

				// Stored in radians, edited in degrees.
				glm::vec3 rotation = glm::degrees(transform.Rotation);
				if (DrawVec3Control("Rotation", rotation, 0.0f))
				{
					transform.Rotation = glm::radians(rotation);
				}

				DrawVec3Control("Scale", transform.Scale, 1.0f);

				// The values are local: relative to the parent (S3).
				if (Entity parent = entity.GetScene()->GetParent(entity))
				{
					ImGui::TextDisabled("Relative to its parent, %s.", parent.GetName().c_str());
					if (HasPhysicsBody(entity) && HasNonUniformlyScaledParent(entity))
					{
						ImGui::TextWrapped("A parent has a non-uniform scale: when rotated, this entity's collider can't match what is drawn. Keep physics bodies under parents with an even scale.");
					}
				}
			});
		}

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, removeSprite, [&](SpriteRendererComponent& sprite)
		{
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##Color", glm::value_ptr(sprite.Color)); });
			DrawPropertyRow("Texture", [&]() { DrawTextureField("##Texture", sprite.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(sprite.Texture);
			const bool isSheet = texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple;
			if (isSheet)
			{
				DrawPropertyRow("Sprite", [&]() { DrawSpriteField(sprite.SpriteIndex, *texture); });
			}
			else
			{
				DrawPropertyRow("Tiling", [&]() { InputFloatField("##Tiling", sprite.TilingFactor, "%.2f", 0.0f, 100.0f); });
			}
			DrawPropertyRow("Flip", [&]()
			{
				ImGui::Checkbox("X##FlipX", &sprite.FlipX);
				ImGui::SameLine();
				ImGui::Checkbox("Y##FlipY", &sprite.FlipY);
			});
			DrawPropertyRow("Facing", [&]()
			{
				const char* facingNames[] = { "Flat", "Upright", "Billboard" };
				int facing = (int)sprite.Facing;
				if (ImGui::Combo("##Facing", &facing, facingNames, 3))
				{
					sprite.Facing = (SpriteFacing)facing;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How the sprite turns towards a tilted camera.\nFlat: lies on the ground (roads, decals, water).\nUpright: stands on its bottom edge and faces the camera (trees, houses, characters).\nBillboard: faces the camera about its centre (glows, icons, world-space UI).\nA top-down camera draws all three the same.");
				}
			});
			DrawPropertyRow("Alpha", [&]()
			{
				const char* alphaNames[] = { "Default", "Cut-out", "Translucent" };
				int alpha = (int)sprite.AlphaMode;
				if (ImGui::Combo("##AlphaMode", &alpha, alphaNames, 3))
				{
					sprite.AlphaMode = (SpriteAlphaMode)alpha;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How the sprite is drawn with shared depth (Render Settings > Rendering).\nDefault: the render features' default sprite alpha.\nCut-out: half-transparent pixels are dropped, the rest hide what is behind (trees, characters).\nTranslucent: blended and sorted back to front (glows, water, smoke).\nWithout shared depth every sprite is translucent.");
				}
			});
			DrawPropertyRow("Overlay", [&]()
			{
				ImGui::Checkbox("##Overlay", &sprite.Overlay);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("HUD sprite: drawn last, on top of everything, unlit and never hidden by depth\n(world-space prompts and menus).");
				}
			});
			DrawPropertyRow("Cast shadows", [&]()
			{
				ImGui::Checkbox("##SpriteCastShadows", &sprite.CastShadows);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Sun shadows (Render Settings > Rendering: Shadows = Sun): a cut-out Upright or Billboard\nsprite casts one, as set by Sprite shadows. Off for grass and flowers.");
				}
			});
			DrawPropertyRow("Emission", [&]()
			{
				ImGui::ColorEdit3("##SpriteEmissionColor", glm::value_ptr(sprite.EmissionColor));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Multiplies the sprite's own colours (white = the texture's colours) for its glow.");
				}
			});
			DrawPropertyRow("Emission intensity", [&]()
			{
				InputFloatField("##SpriteEmissionIntensity", sprite.EmissionIntensity, "%.2f", 0.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The sprite glows: its colours times Emission times this are added after the lighting,\nso it stays bright at night and in shadow (flames, candles, lit windows). 0 = none.\nAbove about 1 it blooms (Render Settings > Rendering: HDR and Bloom). The whole sprite glows:\ngive a torch's flame its own sprite.");
				}
			});
			if (texture != nullptr && entity.HasComponent<TransformComponent>())
			{
				DrawPropertyRow("##NativeSize", [&]()
				{
					if (ImGui::Button("Set Native Size", { ImGui::GetContentRegionAvail().x, 0.0f }))
					{
						// Getting another component doesn't move this one, so `sprite` stays valid.
						const glm::vec2 size = texture->GetSpriteSizeInUnits(sprite.SpriteIndex);
						TransformComponent& transform = entity.GetComponent<TransformComponent>();
						transform.Scale.x = size.x;
						transform.Scale.y = size.y;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Scale the entity to the sprite's size: pixels / the texture's Pixels Per Unit.");
					}
				});
			}
			DrawPropertyRow("Sort Order", [&]()
			{
				int sortOrder = (int)sprite.SortOrder;
				if (ImGui::InputInt("##SortOrder", &sortOrder))
				{
					sprite.SortOrder = (int32_t)sortOrder;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Higher values are drawn on top. Equal values: Flat sprites first (lower Z first),\nthen Upright and Billboard ones from far to near.");
				}
			});
		});

		bool showStartFrame = false;
		DrawComponent<SpriteAnimatorComponent>("Sprite Animator", entity, removeAnimator, [&](SpriteAnimatorComponent& animator)
		{
			const AssetHandle before = animator.Animations;
			DrawPropertyRow("Animations", [&]() { DrawAnimationSetField(entity, animator); });
			DrawPropertyRow("Start Clip", [&]() { showStartFrame |= DrawStartClipField(animator); });
			DrawPropertyRow("Play On Start", [&]() { ImGui::Checkbox("##PlayOnStart", &animator.PlayOnStart); });
			DrawPropertyRow("Speed", [&]() { InputFloatField("##Speed", animator.Speed, "%.2f", 0.0f, 100.0f); });
			showStartFrame |= animator.Animations != before;

			// Playback status (play mode): read through SpriteAnimator, never from the component's State.
			const std::string_view currentClip = SpriteAnimator::GetCurrentClip(entity);
			if (currentClip.empty() == false)
			{
				const char* status = SpriteAnimator::IsPlaying(entity) ? "" : (SpriteAnimator::IsFinished(entity) ? " (finished)" : " (stopped)");
				ImGui::TextDisabled("Playing %.*s, frame %u%s", (int)currentClip.size(), currentClip.data(), SpriteAnimator::GetFrameIndex(entity), status);
			}
			if (entity.HasComponent<SpriteRendererComponent>() == false)
			{
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Needs a Sprite Renderer to show anything.");
			}
		});
		// Outside the section's lambda: it reads another component of the entity.
		if (showStartFrame)
		{
			ShowStartFrame(entity, entity.GetComponent<SpriteAnimatorComponent>());
		}

		DrawComponent<CameraComponent>("Camera", entity, removeCamera, [&](CameraComponent& cameraComponent)
		{
			SceneCamera& camera = cameraComponent.Camera;

			DrawPropertyRow("Primary", [&]() { makePrimary = ImGui::Checkbox("##Primary", &cameraComponent.Primary) && cameraComponent.Primary; });

			const char* projectionNames[] = { "Perspective", "Orthographic" };
			int projection = (int)camera.Projection;
			DrawPropertyRow("Projection", [&]()
			{
				if (ImGui::Combo("##Projection", &projection, projectionNames, 2))
					camera.Projection = (ProjectionType)projection;
			});

			if (camera.Projection == ProjectionType::Perspective)
			{
				DrawPropertyRow("Vertical FOV", [&]() { InputFloatField("##FOV", camera.PerspectiveFOV, "%.1f deg", 1.0f, 179.0f); });
				DrawPropertyRow("Near", [&]() { InputFloatField("##Near", camera.PerspectiveNear, "%.3f", 0.001f, camera.PerspectiveFar); });
				DrawPropertyRow("Far", [&]() { InputFloatField("##Far", camera.PerspectiveFar, "%.1f", camera.PerspectiveNear, 100000.0f); });
			}
			else
			{
				DrawPropertyRow("Size", [&]() { InputFloatField("##Size", camera.OrthographicSize, "%.2f", 0.01f, 10000.0f); });
				DrawPropertyRow("Near", [&]() { InputFloatField("##Near", camera.OrthographicNear, "%.2f", -10000.0f, camera.OrthographicFar); });
				DrawPropertyRow("Far", [&]() { InputFloatField("##Far", camera.OrthographicFar, "%.2f", camera.OrthographicNear, 10000.0f); });
			}
		});

		DrawComponent<Light2DComponent>("Light 2D", entity, removeLight, [&](Light2DComponent& light)
		{
			const char* typeNames[] = { "Point", "Global", "Spot" };
			int type = (int)light.Type;
			DrawPropertyRow("Type", [&]()
			{
				if (ImGui::Combo("##LightType", &type, typeNames, 3))
				{
					light.Type = (Light2DType)type;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Spot lights shine only under forward lighting (Render Settings > Lighting: Forward).\nThey point along the entity's -Z, turned by its rotation.");
				}
			});

			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit3("##LightColor", glm::value_ptr(light.Color)); });
			DrawPropertyRow("Intensity", [&]() { InputFloatField("##LightIntensity", light.Intensity, "%.2f", 0.0f, 100.0f); });

			if (light.Type == Light2DType::Point || light.Type == Light2DType::Spot)
			{
				DrawPropertyRow(light.Type == Light2DType::Spot ? "Range" : "Radius", [&]() { InputFloatField("##LightRadius", light.Radius, "%.2f", 0.01f, 10000.0f); });
				DrawPropertyRow("Height", [&]()
				{
					InputFloatField("##LightHeight", light.Height, "%.2f", 0.0f, 10000.0f);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("How far above its position the light is (a lantern's glass).\nForward lighting: that far up along Z. Light buffer: that far up the screen top-down, up the standing sprite with a tilted camera.");
					}
				});
				if (light.Type == Light2DType::Spot)
				{
					DrawPropertyRow("Inner Angle", [&]() { InputFloatField("##LightInner", light.InnerAngle, "%.1f deg", 0.0f, 89.0f); });
					DrawPropertyRow("Outer Angle", [&]() { InputFloatField("##LightOuter", light.OuterAngle, "%.1f deg", light.InnerAngle, 89.5f); });
				}
			}
			else
			{
				ImGui::TextDisabled("Lights everything equally: the ambient color.");
			}
		});

		DrawComponent<DirectionalLightComponent>("Directional Light", entity, removeDirectionalLight, [&](DirectionalLightComponent& light)
		{
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit3("##SunColor", glm::value_ptr(light.Color)); });
			DrawPropertyRow("Intensity", [&]() { InputFloatField("##SunIntensity", light.Intensity, "%.2f", 0.0f, 100.0f); });
			DrawPropertyRow("Main Light", [&]()
			{
				ImGui::Checkbox("##SunMain", &light.IsMainLight);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The sun of the scene's forward lighting: the one marked, else the first.\nIt shines along the entity's -Z, turned by its rotation.");
				}
			});
			DrawPropertyRow("Cast Shadows", [&]()
			{
				ImGui::Checkbox("##SunCastShadows", &light.CastShadows);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The main light's shadow map, when the render features have Shadows = Sun.");
				}
			});
			ImGui::BeginDisabled(light.CastShadows == false);
			DrawPropertyRow("Shadow Strength", [&]()
			{
				InputFloatField("##SunShadowStrength", light.ShadowStrength, "%.2f", 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("1: no direct sun in the shadow (ambient and lamps still reach it). Lower: lighter shadows.");
				}
			});
			DrawPropertyRow("Shadow Distance", [&]()
			{
				InputFloatField("##SunShadowDistance", light.ShadowDistance, "%.1f m", 1.0f, 500.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Metres around what the camera looks at that get shadows (the map's half width).\nSmaller: sharper shadows over less of the scene.");
				}
			});
			DrawPropertyRow("Shadow Bias", [&]()
			{
				InputFloatField("##SunShadowBias", light.ShadowBias, "%.2f", 0.0f, 10.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How far surfaces are pushed out of their own shadow. Raise it for speckled\n(acne) surfaces, lower it when shadows float off the objects casting them.");
				}
			});
			ImGui::EndDisabled();
			ImGui::TextDisabled("Shines only under forward lighting (Render Settings > Lighting: Forward).");
		});

		DrawComponent<MeshComponent>("Mesh", entity, removeMesh, [&](MeshComponent& mesh)
		{
			const Ref<Mesh> model = (uint64_t)mesh.Model != 0 ? AssetManager::GetAsset<Mesh>(mesh.Model) : nullptr;
			DrawPropertyRow("Model", [&]()
			{
				const std::string detail = model != nullptr
					? std::to_string(model->GetSubMeshes().size()) + " sub-meshes, " + std::to_string(model->GetIndexCount() / 3) + " triangles, "
						+ std::to_string(model->GetMaterialSlotCount()) + " material slots"
					: ((uint64_t)mesh.Model != 0 ? "The model is missing or couldn't be imported (see the console)." : "Empty: a built-in shape.");
				const AssetPickerResult result = AssetPicker::DrawField("##MeshModel", mesh.Model, AssetType::Mesh, detail);
				if (result.Changed == false)
				{
					return;
				}
				if ((uint64_t)result.Handle == 0)
				{
					mesh.Model = 0;
					return;
				}
				// Importing logs why a model can't be used; a handle that doesn't import is not taken.
				const Ref<Mesh> picked = AssetManager::GetAsset<Mesh>(result.Handle);
				if (picked != nullptr)
				{
					mesh.Model = result.Handle;
					// Its own materials until the slots are made here.
					mesh.Materials.clear();
				}
			});
			if ((uint64_t)mesh.Model != 0)
			{
				ImGui::Separator();
				if (mesh.Materials.empty())
				{
					ImGui::TextDisabled("Uses the model's materials (the .hymat files its import made).");
					if (model != nullptr && ImGui::Button("Choose materials here"))
					{
						AssignModelMaterials(mesh, *model);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Gives this mesh a slot per material, starting with the model's, so one slot can use another material.");
					}
				}
				else
				{
					DrawMeshMaterialSlots(entity, mesh, model.get());
					if (ImGui::Button("Use the model's materials"))
					{
						mesh.Materials.clear();
					}
				}
				DrawPropertyRow("Cast shadows", [&]() { ImGui::Checkbox("##MeshCastShadows", &mesh.CastShadows); });
				ImGui::TextDisabled("Drawn with Depth = Shared and Meshes on (Render Settings > Rendering).");
				return;
			}

			DrawPropertyRow("Shape", [&]()
			{
				const char* shapeNames[] = { "Box", "Plane", "Ramp", "Stairs", "Cylinder" };
				int shape = (int)mesh.Shape;
				if (ImGui::Combo("##MeshShape", &shape, shapeNames, 5))
				{
					mesh.Shape = (MeshShape)shape;
				}
			});
			DrawVec3Control("Size", mesh.Size, 1.0f);
			if (mesh.Shape == MeshShape::Stairs)
			{
				DrawPropertyRow("Steps", [&]()
				{
					int steps = (int)mesh.Steps;
					if (ImGui::InputInt("##MeshSteps", &steps))
					{
						mesh.Steps = (uint32_t)std::clamp(steps, 1, 64);
					}
				});
			}
			if (mesh.Shape == MeshShape::Cylinder)
			{
				DrawPropertyRow("Segments", [&]()
				{
					int segments = (int)mesh.Segments;
					if (ImGui::InputInt("##MeshSegments", &segments))
					{
						mesh.Segments = (uint32_t)std::clamp(segments, 3, 128);
					}
				});
			}
			ImGui::TextDisabled("Metres, Z up, standing on the entity's position.");

			if (mesh.Materials.empty())
			{
				mesh.Materials.push_back(MeshMaterialSlot{});
			}
			DrawMeshMaterialSlots(entity, mesh, nullptr);
			DrawPropertyRow("Cast shadows", [&]() { ImGui::Checkbox("##MeshCastShadows", &mesh.CastShadows); });
			ImGui::TextDisabled("Drawn with Depth = Shared and Meshes on (Render Settings > Rendering).");
		});

		DrawComponent<Shadow2DComponent>("Shadow 2D", entity, removeShadow, [&](Shadow2DComponent& shadow)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##ShadowOffsetX", shadow.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##ShadowOffsetY", shadow.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Width", [&]() { InputFloatField("##ShadowWidth", shadow.Size.x, "%.2f", 0.01f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##ShadowHeight", shadow.Size.y, "%.2f", 0.01f, 10000.0f); });
			DrawPropertyRow("Strength", [&]() { InputFloatField("##ShadowStrength", shadow.Strength, "%.2f", 0.0f, 1.0f); });
			ImGui::TextDisabled("A blob under the entity. Shows in views that have lights.");
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, removeRigidbody, [&](Rigidbody2DComponent& body)
		{
			const char* typeNames[] = { "Static", "Dynamic", "Kinematic" };
			int type = (int)body.Type;
			DrawPropertyRow("Type", [&]()
			{
				if (ImGui::Combo("##BodyType", &type, typeNames, 3))
				{
					body.Type = (Rigidbody2DType)type;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Static: never moves.\nDynamic: moved by forces, gravity and collisions.\nKinematic: moved only by the game; pushes dynamic bodies.");
				}
			});
			DrawPropertyRow("Gravity Scale", [&]() { InputFloatField("##GravityScale", body.GravityScale, "%.2f", -100.0f, 100.0f); });
			DrawPropertyRow("Linear Damping", [&]() { InputFloatField("##LinearDamping", body.LinearDamping, "%.2f", 0.0f, 1000.0f); });
			DrawPropertyRow("Angular Damping", [&]() { InputFloatField("##AngularDamping", body.AngularDamping, "%.2f", 0.0f, 1000.0f); });
			DrawPropertyRow("Fixed Rotation", [&]() { ImGui::Checkbox("##FixedRotation", &body.FixedRotation); });
			DrawPropertyRow("Bullet", [&]()
			{
				ImGui::Checkbox("##IsBullet", &body.IsBullet);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Continuous collision for fast, small bodies such as projectiles. Costs more.");
				}
			});
		});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, removeBoxCollider, [&](BoxCollider2DComponent& collider)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##BoxOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##BoxOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Width", [&]() { InputFloatField("##BoxWidth", collider.Size.x, "%.2f", 0.001f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##BoxHeight", collider.Size.y, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
			}
		});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, removeCircleCollider, [&](CircleCollider2DComponent& collider)
		{
			DrawPropertyRow("Offset X", [&]() { InputFloatField("##CircleOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Offset Y", [&]() { InputFloatField("##CircleOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Radius", [&]() { InputFloatField("##CircleRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
			}
		});

		DrawComponent<BoxColliderComponent>("Box Collider", entity, removeBoxCollider3D, [&](BoxColliderComponent& collider)
		{
			DrawVec3Control("Offset", collider.Offset);
			DrawVec3Control("Size", collider.Size, 1.0f);
			collider.Size = glm::max(collider.Size, glm::vec3(0.001f));
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});

		DrawComponent<SphereColliderComponent>("Sphere Collider", entity, removeSphereCollider, [&](SphereColliderComponent& collider)
		{
			DrawVec3Control("Offset", collider.Offset);
			DrawPropertyRow("Radius", [&]() { InputFloatField("##SphereRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});

		DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, removeCapsuleCollider, [&](CapsuleColliderComponent& collider)
		{
			DrawVec3Control("Offset", collider.Offset);
			DrawPropertyRow("Radius", [&]() { InputFloatField("##CapsuleRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##CapsuleHeight", collider.Height, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});

		DrawComponent<CylinderColliderComponent>("Cylinder Collider", entity, removeCylinderCollider, [&](CylinderColliderComponent& collider)
		{
			DrawVec3Control("Offset", collider.Offset);
			DrawPropertyRow("Radius", [&]() { InputFloatField("##CylinderRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##CylinderHeight", collider.Height, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});

		DrawComponent<MeshColliderComponent>("Mesh Collider", entity, removeMeshCollider, [&](MeshColliderComponent& collider)
		{
			DrawPhysicsLayerRow(collider.Layer);
			if (entity.HasComponent<MeshComponent>() == false)
			{
				ImGui::TextDisabled("Needs a Mesh on the entity.");
			}
			else if ((uint64_t)entity.GetComponent<MeshComponent>().Model != 0)
			{
				ImGui::TextDisabled("A model collides as its bounding box for now.");
			}
			else
			{
				ImGui::TextDisabled("The built-in shape collides as itself (stairs as a ramp).");
			}
		});

		DrawComponent<CharacterBodyComponent>("Character Body", entity, removeCharacterBody, [&](CharacterBodyComponent& body)
		{
			DrawPropertyRow("Radius", [&]() { InputFloatField("##CharacterRadius", body.Radius, "%.2f", 0.05f, 100.0f); });
			DrawPropertyRow("Height", [&]() { InputFloatField("##CharacterHeight", body.Height, "%.2f", 0.2f, 100.0f); });
			DrawPropertyRow("Max Slope", [&]() { InputFloatField("##CharacterMaxSlope", body.MaxSlopeDegrees, "%.1f", 0.0f, 89.0f); });
			DrawPropertyRow("Step Height", [&]() { InputFloatField("##CharacterStepHeight", body.StepHeight, "%.2f", 0.0f, 10.0f); });
			DrawPropertyRow("Stays On Ledges", [&]()
			{
				ImGui::Checkbox("##CharacterStaysOnLedges", &body.StaysOnLedges);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Never walks off a drop higher than Step Height: it stops at the edge instead of falling.");
				}
			});
			DrawPhysicsLayerRow(body.Layer);
			ImGui::TextDisabled("A capsule on the entity's feet; scripts move it with CharacterBody::Move.");
		});

		DrawComponent<ParticleEmitter2DComponent>("Particle Emitter 2D", entity, removeEmitter, [&](ParticleEmitter2DComponent& emitter)
		{
			const float limit = (float)ParticleEmitter2DComponent::MaxParticlesLimit;

			DrawSubheading("Emission");
			DrawPropertyRow("Play On Start", [&]() { ImGui::Checkbox("##EmitterPlayOnStart", &emitter.PlayOnStart); });
			DrawPropertyRow("Looping", [&]() { ImGui::Checkbox("##EmitterLooping", &emitter.Looping); });
			DrawPropertyRow("Duration", [&]() { InputFloatField("##EmitterDuration", emitter.Duration, "%.2f s", 0.01f, 3600.0f); });
			DrawPropertyRow("Rate", [&]() { InputFloatField("##EmitterRate", emitter.Rate, "%.1f / s", 0.0f, 10000.0f); });
			DrawPropertyRow("Burst", [&]() { PropertyUI::InputUInt("##EmitterBurst", emitter.BurstCount, 0, (uint32_t)limit); });
			DrawPropertyRow("Max Particles", [&]() { PropertyUI::InputUInt("##EmitterMaxParticles", emitter.MaxParticles, 1, (uint32_t)limit); });
			DrawPropertyRow("Seed", [&]()
			{
				PropertyUI::InputUInt("##EmitterSeed", emitter.Seed, 0, 1000000000u);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("0: a new random seed each time it plays. Any other value: the same pattern every time.");
				}
			});
			const char* shapeNames[] = { "Point", "Circle", "Box" };
			int shape = (int)emitter.Shape;
			DrawPropertyRow("Shape", [&]()
			{
				if (ImGui::Combo("##EmitterShape", &shape, shapeNames, 3))
				{
					emitter.Shape = (EmitterShape)shape;
				}
			});
			if (emitter.Shape == EmitterShape::Circle)
			{
				DrawPropertyRow("Radius", [&]() { InputFloatField("##EmitterRadius", emitter.ShapeSize.x, "%.2f", 0.0f, 10000.0f); });
			}
			else if (emitter.Shape == EmitterShape::Box)
			{
				DrawPropertyRow("Width", [&]() { InputFloatField("##EmitterWidth", emitter.ShapeSize.x, "%.2f", 0.0f, 10000.0f); });
				DrawPropertyRow("Height", [&]() { InputFloatField("##EmitterHeight", emitter.ShapeSize.y, "%.2f", 0.0f, 10000.0f); });
			}

			DrawSubheading("Particles at birth");
			DrawPropertyRow("Lifetime", [&]() { DrawRangeField("EmitterLifetime", emitter.Lifetime, 0.01f, 3600.0f); });
			DrawPropertyRow("Speed", [&]() { DrawRangeField("EmitterSpeed", emitter.Speed, -10000.0f, 10000.0f); });
			DrawPropertyRow("Direction", [&]() { InputFloatField("##EmitterDirection", emitter.Direction, "%.1f deg", -360.0f, 360.0f); });
			DrawPropertyRow("Spread", [&]() { InputFloatField("##EmitterSpread", emitter.Spread, "%.1f deg", 0.0f, 180.0f); });
			DrawPropertyRow("Size", [&]() { DrawRangeField("EmitterSize", emitter.StartSize, 0.0f, 10000.0f); });
			DrawPropertyRow("Rotation", [&]() { DrawRangeField("EmitterRotation", emitter.StartRotation, -360.0f, 360.0f, "%.1f"); });
			DrawPropertyRow("Spin", [&]() { DrawRangeField("EmitterSpin", emitter.AngularVelocity, -10000.0f, 10000.0f, "%.1f"); });

			DrawSubheading("Over lifetime");
			DrawPropertyRow("Start Color", [&]() { ImGui::ColorEdit4("##EmitterStartColor", glm::value_ptr(emitter.StartColor)); });
			DrawPropertyRow("End Color", [&]() { ImGui::ColorEdit4("##EmitterEndColor", glm::value_ptr(emitter.EndColor)); });
			DrawPropertyRow("End Size", [&]() { InputFloatField("##EmitterEndSize", emitter.EndSizeScale, "x %.2f", 0.0f, 100.0f); });
			DrawPropertyRow("Gravity X", [&]() { InputFloatField("##EmitterGravityX", emitter.Gravity.x, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Gravity Y", [&]() { InputFloatField("##EmitterGravityY", emitter.Gravity.y, "%.2f", -10000.0f, 10000.0f); });
			DrawPropertyRow("Drag", [&]() { InputFloatField("##EmitterDrag", emitter.Drag, "%.2f", 0.0f, 1000.0f); });
			DrawPropertyRow("Turbulence", [&]() { InputFloatField("##EmitterTurbulence", emitter.Turbulence, "%.2f", 0.0f, 10000.0f); });

			DrawSubheading("Look");
			DrawPropertyRow("Texture", [&]() { DrawTextureField("##EmitterTexture", emitter.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(emitter.Texture);
			if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
			{
				DrawPropertyRow("Sprite", [&]() { PropertyUI::InputUInt("##EmitterSprite", emitter.SpriteIndex, 0, texture->GetSpriteCount() > 0 ? texture->GetSpriteCount() - 1 : 0); });
			}
			const char* blendNames[] = { "Alpha", "Additive" };
			int blend = (int)emitter.Blend;
			DrawPropertyRow("Blend", [&]()
			{
				if (ImGui::Combo("##EmitterBlend", &blend, blendNames, 2))
				{
					emitter.Blend = (ParticleBlend)blend;
				}
			});
			const char* lightingNames[] = { "Lit", "Unlit" };
			int lighting = (int)emitter.Lighting;
			DrawPropertyRow("Lighting", [&]()
			{
				if (ImGui::Combo("##EmitterLighting", &lighting, lightingNames, 2))
				{
					emitter.Lighting = (ParticleLighting)lighting;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Unlit: not darkened by scene lighting. Unlit + Additive stays bright at night.");
				}
			});
			const char* spaceNames[] = { "Ground", "View" };
			int space = (int)emitter.Space;
			DrawPropertyRow("Space", [&]()
			{
				if (ImGui::Combo("##EmitterSpace", &space, spaceNames, 2))
				{
					emitter.Space = (ParticleSpace)space;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("With a tilted camera: Ground keeps the particles on the ground plane (dust, leaves);\nView sprays them in the plane facing the camera (sparks, impacts).");
				}
			});
			DrawPropertyRow("Sort Order", [&]()
			{
				int sortOrder = (int)emitter.SortOrder;
				if (ImGui::InputInt("##EmitterSortOrder", &sortOrder))
				{
					emitter.SortOrder = (int32_t)sortOrder;
				}
			});

			DrawPropertyRow("Emission", [&]()
			{
				InputFloatField("##EmitterEmissionIntensity", emitter.EmissionIntensity, "%.2f", 0.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Each particle's colour times this is added after the lighting: sparks and embers glow\nat night and bloom above about 1 (HDR and Bloom). 0 = none.");
				}
			});

			DrawSubheading("Light (optional)");
			DrawPropertyRow("Light Radius", [&]() { InputFloatField("##EmitterLightRadius", emitter.LightRadius, "%.2f", 0.0f, 10000.0f); });
			if (emitter.LightRadius > 0.0f)
			{
				DrawPropertyRow("Light Intensity", [&]() { InputFloatField("##EmitterLightIntensity", emitter.LightIntensity, "%.2f", 0.0f, 100.0f); });
				DrawPropertyRow("Max Lights", [&]() { PropertyUI::InputUInt("##EmitterMaxLights", emitter.MaxLights, 0, (uint32_t)limit); });
			}

			DrawSubheading("Cleanup");
			DrawPropertyRow("Destroy When Done", [&]()
			{
				ImGui::Checkbox("##EmitterDestroyWhenFinished", &emitter.DestroyWhenFinished);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("For one-shot effects: the entity is destroyed once it stopped emitting and no particles are left.");
				}
			});

			ImGui::TextDisabled("Settings only for now: particles are simulated and drawn from Roadmap V2.");
		});

		DrawUIComponents(entity, removeUICanvas, removeUIRect, removeUIImage, removeUIText, removeUIButton, removeUILayoutGroup);

		int scriptToRemove = -1;
		DrawComponent<NativeScriptComponent>("Scripts", entity, removeScripts, [&](NativeScriptComponent& scripts)
		{
			for (int i = 0; i < (int)scripts.Scripts.size(); i++)
			{
				NativeScriptComponent::Script& script = scripts.Scripts[i];
				ImGui::PushID(i);

				// Script name + remove button.
				ImGui::AlignTextToFramePadding();
				if (script.Name.empty())
					ImGui::TextDisabled("(bound in code, not saved)");
				else if (script.Instantiate == nullptr)
					ImGui::TextColored({ 1.0f, 0.45f, 0.35f, 1.0f }, "%s (not registered)", script.Name.c_str());
				else
					ImGui::TextUnformatted(script.Name.c_str());

				if (ImGui::IsItemHovered() && script.Instantiate == nullptr && script.Name.empty() == false)
					ImGui::SetTooltip("No script is registered under this name. It is kept in the scene but doesn't run.");

				ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
				if (ImGui::Button("X", { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
					scriptToRemove = i;

				// Exposed script fields are drawn directly under the script name.
				ScriptTypeInfo* typeInfo = ScriptRegistry::FindType(script.Name);
				if (typeInfo != nullptr && typeInfo->Properties.empty() == false)
				{
					ImGui::Spacing();

					// While the game runs, the fields show what the script holds now and edits go straight into it
					// (#146). They are made on the play copy, so Stop discards them like every other Play edit.
					ScriptableEntity* instance = script.Instance.get();
					for (const ScriptProperty& property : typeInfo->Properties)
					{
						if (property.HideInInspector)
						{
							continue;
						}

						// A property the binding has no value for yet (added to the class since), or one whose type
						// changed: start from the default.
						auto valueIt = script.Properties.find(property.Name);
						if (valueIt == script.Properties.end())
						{
							valueIt = script.Properties.emplace(property.Name, property.DefaultValue).first;
						}
						else if (ScriptPropertyHasType(valueIt->second, property.Type) == false)
						{
							valueIt->second = property.DefaultValue;
						}

						ScriptPropertyValue& value = valueIt->second;
						if (instance != nullptr && property.Read != nullptr)
						{
							value = property.Read(*instance);
						}

						if (DrawScriptPropertyField(property, value) && instance != nullptr && property.Apply != nullptr)
						{
							property.Apply(*instance, value);
						}
					}
					if (instance != nullptr)
					{
						ImGui::TextDisabled("Playing: edits apply now and are discarded on Stop.");
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
				ImGui::OpenPopup("AddScript");

			if (ImGui::BeginPopup("AddScript"))
			{
				DrawScriptMenuItems(entity);
				ImGui::EndPopup();
			}
		});

		// Only one primary camera: ticking Primary here unticks it everywhere else.
		if (makePrimary)
		{
			auto cameras = entity.GetScene()->m_Registry.view<CameraComponent>();
			for (auto other : cameras)
			{
				if (other != (entt::entity)entity)
					cameras.get<CameraComponent>(other).Primary = false;
			}
		}

		// Apply removals after drawing, when no component references are alive.
		if (removeTransform)
			entity.RemoveComponent<TransformComponent>();
		if (removeSprite)
			entity.RemoveComponent<SpriteRendererComponent>();
		if (removeCamera)
			entity.RemoveComponent<CameraComponent>();
		if (removeAnimator)
		{
			entity.RemoveComponent<SpriteAnimatorComponent>();
		}
		if (removeLight)
		{
			entity.RemoveComponent<Light2DComponent>();
		}
		if (removeShadow)
		{
			entity.RemoveComponent<Shadow2DComponent>();
		}
		if (removeRigidbody)
		{
			entity.RemoveComponent<Rigidbody2DComponent>();
		}
		if (removeBoxCollider)
		{
			entity.RemoveComponent<BoxCollider2DComponent>();
		}
		if (removeCircleCollider)
		{
			entity.RemoveComponent<CircleCollider2DComponent>();
		}
		if (removeBoxCollider3D)
		{
			entity.RemoveComponent<BoxColliderComponent>();
		}
		if (removeSphereCollider)
		{
			entity.RemoveComponent<SphereColliderComponent>();
		}
		if (removeCapsuleCollider)
		{
			entity.RemoveComponent<CapsuleColliderComponent>();
		}
		if (removeCylinderCollider)
		{
			entity.RemoveComponent<CylinderColliderComponent>();
		}
		if (removeMeshCollider)
		{
			entity.RemoveComponent<MeshColliderComponent>();
		}
		if (removeCharacterBody)
		{
			entity.RemoveComponent<CharacterBodyComponent>();
		}
		if (removeEmitter)
		{
			entity.RemoveComponent<ParticleEmitter2DComponent>();
		}
		if (removeMesh)
		{
			entity.RemoveComponent<MeshComponent>();
		}
		if (removeDirectionalLight)
		{
			entity.RemoveComponent<DirectionalLightComponent>();
		}
		if (removeUICanvas)
		{
			entity.RemoveComponent<UICanvasComponent>();
		}
		if (removeUIRect)
		{
			entity.RemoveComponent<UIRectComponent>();
		}
		if (removeUIImage)
		{
			entity.RemoveComponent<UIImageComponent>();
		}
		if (removeUIText)
		{
			entity.RemoveComponent<UITextComponent>();
		}
		if (removeUIButton)
		{
			entity.RemoveComponent<UIButtonComponent>();
		}
		if (removeUILayoutGroup)
		{
			entity.RemoveComponent<UILayoutGroupComponent>();
		}

		// Scripts that are running (play mode) get OnDestroy before they go.
		if (removeScripts)
			entity.GetScene()->RemoveScripts(entity);
		else if (scriptToRemove >= 0)
			entity.GetScene()->RemoveScript(entity, (size_t)scriptToRemove);
	}

	// ---- Runtime UI --------------------------------------------------------------------------------

	// The canvas the entity is drawn on: the nearest ancestor with a UICanvasComponent (the entity itself counts).
	// `nested` is set when that canvas is inside another one (not supported: it isn't drawn).
	static Entity FindUICanvas(Entity entity, bool& nested)
	{
		nested = false;
		Scene* scene = entity.GetScene();
		Entity canvas;
		for (Entity current = entity; (bool)current; current = scene->GetParent(current))
		{
			if (current.HasComponent<UICanvasComponent>())
			{
				if ((bool)canvas)
				{
					nested = true;
					break;
				}
				canvas = current;
			}
		}
		return canvas;
	}

	// One axis of the anchor presets: Unity's left / centre / right / stretch (min, max).
	struct UIAnchorPreset
	{
		const char* Name;
		float Min;
		float Max;
	};

	static const UIAnchorPreset s_HorizontalAnchors[4] = { { "Left", 0.0f, 0.0f }, { "Center", 0.5f, 0.5f }, { "Right", 1.0f, 1.0f }, { "Stretch", 0.0f, 1.0f } };
	static const UIAnchorPreset s_VerticalAnchors[4] = { { "Top", 1.0f, 1.0f }, { "Middle", 0.5f, 0.5f }, { "Bottom", 0.0f, 0.0f }, { "Stretch", 0.0f, 1.0f } };

	// The preset an axis matches, or -1 (custom anchors).
	static int FindAnchorPreset(const UIAnchorPreset (&presets)[4], float min, float max)
	{
		for (int i = 0; i < 4; i++)
		{
			if (std::abs(presets[i].Min - min) < 1.0e-4f && std::abs(presets[i].Max - max) < 1.0e-4f)
			{
				return i;
			}
		}
		return -1;
	}

	// One axis of a preset onto the rect. A stretched axis fills the parent (no extra size, no offset). With
	// `setPivot` (Alt) the pivot goes to the anchors too and the element moves onto them.
	static void ApplyAnchorPreset(UIRectComponent& rect, int axis, const UIAnchorPreset& preset, bool setPivot)
	{
		rect.AnchorMin[axis] = preset.Min;
		rect.AnchorMax[axis] = preset.Max;
		const bool stretch = preset.Max > preset.Min;
		if (stretch)
		{
			rect.SizeDelta[axis] = 0.0f;
			rect.AnchoredPosition[axis] = 0.0f;
		}
		if (setPivot)
		{
			rect.Pivot[axis] = stretch ? 0.5f : preset.Min;
			rect.AnchoredPosition[axis] = 0.0f;
		}
	}

	// A button showing the rect's anchors that opens the 4 x 4 grid of presets (rows: top, middle, bottom, stretch;
	// columns: left, centre, right, stretch). Hold Alt to set the pivot as well.
	static void DrawAnchorPresets(UIRectComponent& rect)
	{
		const int horizontal = FindAnchorPreset(s_HorizontalAnchors, rect.AnchorMin.x, rect.AnchorMax.x);
		const int vertical = FindAnchorPreset(s_VerticalAnchors, rect.AnchorMin.y, rect.AnchorMax.y);
		char current[64];
		if (horizontal >= 0 && vertical >= 0)
		{
			std::snprintf(current, sizeof(current), "%s %s", s_VerticalAnchors[vertical].Name, s_HorizontalAnchors[horizontal].Name);
		}
		else
		{
			std::snprintf(current, sizeof(current), "Custom");
		}

		if (ImGui::Button(current, { -1.0f, 0.0f }))
		{
			ImGui::OpenPopup("AnchorPresets");
		}
		if (ImGui::BeginPopup("AnchorPresets"))
		{
			ImGui::TextDisabled("Anchor presets (hold Alt to set the pivot too)");
			const bool setPivot = ImGui::GetIO().KeyAlt;
			const float cell = ImGui::GetFrameHeight() * 2.0f;
			for (int row = 0; row < 4; row++)
			{
				for (int column = 0; column < 4; column++)
				{
					const UIAnchorPreset& h = s_HorizontalAnchors[column];
					const UIAnchorPreset& v = s_VerticalAnchors[row];
					char label[32];
					std::snprintf(label, sizeof(label), "%c%c##preset%d", v.Name[0], h.Name[0], row * 4 + column);
					const bool selected = row == vertical && column == horizontal;
					if (selected)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
					}
					if (ImGui::Button(label, { cell, cell }))
					{
						ApplyAnchorPreset(rect, 0, h, setPivot);
						ApplyAnchorPreset(rect, 1, v, setPivot);
						ImGui::CloseCurrentPopup();
					}
					if (selected)
					{
						ImGui::PopStyleColor();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s, %s", v.Name, h.Name);
					}
					if (column < 3)
					{
						ImGui::SameLine();
					}
				}
			}
			ImGui::EndPopup();
		}
	}

	// A button's explicit neighbour: one of the other buttons on the same canvas, by name; (none) clears it.
	static void DrawButtonReferenceField(const char* id, Entity entity, UUID& reference)
	{
		Scene& scene = *entity.GetScene();
		bool nested = false;
		const Entity canvas = UI::FindCanvas(entity, nested);
		Entity current = (uint64_t)reference != 0 ? scene.GetEntityByUUID(reference) : Entity{};
		const std::string preview = (bool)current ? current.GetName() : ((uint64_t)reference != 0 ? std::string("(missing)") : std::string("(none)"));
		if (ImGui::BeginCombo(id, preview.c_str()))
		{
			if (ImGui::Selectable("(none)", (uint64_t)reference == 0))
			{
				reference = 0;
			}
			auto buttons = scene.m_Registry.view<UIButtonComponent>();
			for (entt::entity handle : buttons)
			{
				Entity other{ handle, &scene };
				bool otherNested = false;
				if (other == entity || other.HasComponent<IDComponent>() == false || UI::FindCanvas(other, otherNested) != canvas)
				{
					continue;
				}
				ImGui::PushID((int)(uint32_t)handle);
				if (ImGui::Selectable(other.GetName().c_str(), other == current))
				{
					reference = other.GetUUID();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
	}

	void InspectorWindow::DrawUIComponents(Entity entity, bool& removeCanvas, bool& removeRect, bool& removeImage, bool& removeText, bool& removeButton, bool& removeLayoutGroup)
	{
		DrawComponent<UICanvasComponent>("UI Canvas", entity, removeCanvas, [&](UICanvasComponent& canvas)
		{
			DrawVectorControl("Reference", glm::value_ptr(canvas.ReferenceResolution), 2, 0.0f, 1.0f, 16384.0f);
			DrawPropertyRow("Scale Mode", [&]()
			{
				const char* modes[] = { "Integer scale", "Scale with screen", "Constant pixel size" };
				int mode = (int)canvas.ScaleMode;
				if (ImGui::Combo("##ScaleMode", &mode, modes, 3))
				{
					canvas.ScaleMode = (UIScaleMode)mode;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Integer scale: the largest whole multiple of the reference that fits, at least 1 (pixel UI stays crisp);\nthe leftover space widens the canvas, and a view smaller than the reference crops it.\nScale with screen: a fractional scale that fits the reference (smooth UI).\nConstant pixel size: one canvas pixel per view pixel.");
				}
			});
			DrawPropertyRow("Space", [&]()
			{
				const char* spaces[] = { "Screen", "World" };
				int space = (int)canvas.Space;
				if (ImGui::Combo("##CanvasSpace", &space, spaces, 2))
				{
					canvas.Space = (UICanvasSpace)space;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Screen: covers the view.\nWorld: a box of World Size canvas pixels pinned to this entity's position (where the\nprimary camera sees it), the same size on screen at any distance: labels over characters, prompts.\nGive the entity a Transform, or put it under the character it follows.");
				}
			});
			if (canvas.Space == UICanvasSpace::World)
			{
				DrawVectorControl("World Size", glm::value_ptr(canvas.WorldSize), 2, 100.0f, 1.0f, 16384.0f);
				DrawVectorControl("World Pivot", glm::value_ptr(canvas.WorldPivot), 2, 0.5f, 0.0f, 1.0f);
			}
			DrawPropertyRow("Sort Order", [&]() { ImGui::InputInt("##CanvasSortOrder", &canvas.SortOrder); });
			DrawPropertyRow("Visible", [&]() { ImGui::Checkbox("##CanvasVisible", &canvas.Visible); });
			DrawPropertyRow("Blocks Input", [&]()
			{
				ImGui::Checkbox("##CanvasBlocksInput", &canvas.BlocksGameplayInput);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("While the canvas is visible, gameplay scripts don't get keyboard input (menus, dialogue).");
				}
			});
			DrawPropertyRow("Draw Debug", [&]() { ImGui::Checkbox("##CanvasDrawDebug", &canvas.DrawDebug); });

			Scene* scene = entity.GetScene();
			const glm::vec2 viewSize{ (float)scene->GetViewportWidth(), (float)scene->GetViewportHeight() };
			UICanvasMetrics metrics;
			if (scene->GetUICanvasMetrics(entity.GetUUID(), metrics) == false)
			{
				metrics = UILayout::GetCanvasMetrics(canvas, viewSize);
				if (canvas.Space == UICanvasSpace::World)
				{
					metrics.Size = canvas.WorldSize;
				}
			}
			ImGui::TextDisabled("Game view %.0f x %.0f: canvas %.0f x %.0f at x%.2f", viewSize.x, viewSize.y, metrics.Size.x, metrics.Size.y, metrics.Scale);
			bool nested = false;
			FindUICanvas(entity, nested);
			if (nested)
			{
				ImGui::TextWrapped("This canvas is inside another canvas. Nested canvases aren't supported: it isn't drawn.");
			}
		});

		DrawComponent<UIRectComponent>("UI Rect", entity, removeRect, [&](UIRectComponent& rect)
		{
			// Under a layout group only the size counts: the group places the element (UI4).
			const Entity parent = entity.GetScene()->GetParent(entity);
			const bool driven = (bool)parent && parent.HasComponent<UILayoutGroupComponent>() && parent.HasComponent<UIRectComponent>();
			if (driven)
			{
				ImGui::TextWrapped("Driven by layout group %s: it places this element; only Size (along the stack) and Visible count.", parent.GetName().c_str());
			}
			ImGui::BeginDisabled(driven);
			DrawPropertyRow("Anchors", [&]() { DrawAnchorPresets(rect); });
			DrawVectorControl("Position", glm::value_ptr(rect.AnchoredPosition), 2, 0.0f);
			ImGui::EndDisabled();
			// Added to the anchors' span: the size itself when the anchors meet, a margin on a stretched axis.
			DrawVectorControl("Size", glm::value_ptr(rect.SizeDelta), 2, 100.0f);
			ImGui::BeginDisabled(driven);
			DrawVectorControl("Anchor Min", glm::value_ptr(rect.AnchorMin), 2, 0.5f, 0.0f, 1.0f);
			DrawVectorControl("Anchor Max", glm::value_ptr(rect.AnchorMax), 2, 0.5f, 0.0f, 1.0f);
			DrawVectorControl("Pivot", glm::value_ptr(rect.Pivot), 2, 0.5f);
			ImGui::EndDisabled();
			DrawPropertyRow("Visible", [&]() { ImGui::Checkbox("##RectVisible", &rect.Visible); });

			bool nested = false;
			const Entity canvas = FindUICanvas(entity, nested);
			if ((bool)canvas == false)
			{
				ImGui::TextWrapped("No canvas above this element: it isn't drawn. Put it under an entity with a UI Canvas.");
			}
			else if (nested)
			{
				ImGui::TextWrapped("Its canvas is inside another canvas. Nested canvases aren't supported: it isn't drawn.");
			}
			else
			{
				ImGui::TextDisabled("Canvas pixels, Y up, on %s. Size adds to the anchors' span.", canvas.GetName().c_str());
			}
#ifdef HY_DEBUG
			if (entity.HasComponent<UIResolvedRectComponent>())
			{
				const UIResolvedRectComponent& resolved = entity.GetComponent<UIResolvedRectComponent>();
				ImGui::TextDisabled("Resolved (%.0f, %.0f) - (%.0f, %.0f)%s", resolved.Min.x, resolved.Min.y, resolved.Max.x, resolved.Max.y, resolved.Visible ? "" : ", hidden");
			}
#endif
		});

		DrawComponent<UIImageComponent>("UI Image", entity, removeImage, [&](UIImageComponent& image)
		{
			DrawPropertyRow("Texture", [&]() { DrawTextureField("##UIImageTexture", image.Texture); });
			const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(image.Texture);
			if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
			{
				DrawPropertyRow("Sprite", [&]() { DrawSpriteField(image.SpriteIndex, *texture); });
			}
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##UIImageColor", glm::value_ptr(image.Color)); });
			DrawPropertyRow("Slice Left", [&]() { InputFloatField("##SliceLeft", image.NineSlice.x, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Slice Bottom", [&]() { InputFloatField("##SliceBottom", image.NineSlice.y, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Slice Right", [&]() { InputFloatField("##SliceRight", image.NineSlice.z, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Slice Top", [&]() { InputFloatField("##SliceTop", image.NineSlice.w, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Flip", [&]()
			{
				ImGui::Checkbox("X##UIImageFlipX", &image.FlipX);
				ImGui::SameLine();
				ImGui::Checkbox("Y##UIImageFlipY", &image.FlipY);
			});
			ImGui::TextDisabled(UILayout::IsNineSliced(image) ? "Nine-sliced: the borders (texture pixels) keep their size." : "Stretched over the rect (set slice borders for a nine-slice frame).");
		});

		DrawComponent<UITextComponent>("UI Text", entity, removeText, [&](UITextComponent& text)
		{
			DrawPropertyRow("Font", [&]() { DrawFontField("##UITextFont", text.Font); });
			DrawPropertyRow("Text", [&]()
			{
				ImGui::InputTextMultiline("##UIText", text.Text.data(), text.Text.capacity() + 1, { -1.0f, ImGui::GetTextLineHeight() * 4.0f },
					ImGuiInputTextFlags_CallbackResize, StringResizeCallback, &text.Text);
			});
			DrawPropertyRow("Color", [&]() { ImGui::ColorEdit4("##UITextColor", glm::value_ptr(text.Color)); });
			DrawPropertyRow("Scale", [&]()
			{
				int scale = (int)text.Scale;
				if (ImGui::InputInt("##UITextScale", &scale))
				{
					text.Scale = (uint32_t)std::clamp(scale, 1, 64);
				}
			});
			DrawPropertyRow("Align", [&]()
			{
				const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
				const char* horizontal[] = { "Left", "Center", "Right" };
				const char* vertical[] = { "Top", "Middle", "Bottom" };
				int alignH = (int)text.AlignH;
				int alignV = (int)text.AlignV;
				ImGui::SetNextItemWidth(half);
				if (ImGui::Combo("##UITextAlignH", &alignH, horizontal, 3))
				{
					text.AlignH = (TextAlignH)alignH;
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(half);
				if (ImGui::Combo("##UITextAlignV", &alignV, vertical, 3))
				{
					text.AlignV = (TextAlignV)alignV;
				}
			});
			DrawPropertyRow("Wrap", [&]() { ImGui::Checkbox("##UITextWrap", &text.Wrap); });
			DrawPropertyRow("Line Spacing", [&]() { InputFloatField("##UITextLineSpacing", text.LineSpacing, "%.2f", 0.0f, 10.0f); });
			DrawPropertyRow("Shadow", [&]()
			{
				ImGui::ColorEdit4("##UITextShadow", glm::value_ptr(text.ShadowColor));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("A drop shadow under the glyphs (alpha 0 = none): readable text over a busy scene.");
				}
			});
			if (text.ShadowColor.a > 0.0f)
			{
				DrawVectorControl("Shadow Offset", glm::value_ptr(text.ShadowOffset), 2, 0.0f);
			}
			DrawPropertyRow("Visible Chars", [&]()
			{
				int visible = (int)text.VisibleCharacters;
				if (ImGui::InputInt("##UITextVisible", &visible))
				{
					text.VisibleCharacters = std::max(visible, -1);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("-1 shows all of it; a typewriter counts this up without the text moving.");
				}
			});
			const Ref<Font> font = AssetManager::GetLoadedAsset<Font>(text.Font);
			if (font == nullptr)
			{
				ImGui::TextDisabled("Pick a font (.fnt) to draw it.");
			}
			else if (entity.HasComponent<UIResolvedRectComponent>())
			{
				// What the Game view draws, at the rect's current width.
				const UIResolvedRectComponent& rect = entity.GetComponent<UIResolvedRectComponent>();
				TextLayoutProperties settings;
				settings.Scale = text.Scale;
				settings.BoxWidth = rect.Max.x - rect.Min.x;
				settings.Wrap = text.Wrap;
				settings.LineSpacing = text.LineSpacing;
				TextLayoutResult layout;
				TextLayout::Layout(text.Text, font->GetData(), settings, layout);
				ImGui::TextDisabled("%u lines, %u characters, %.0f px tall (rect %.0f)", layout.LineCount, layout.CharacterCount, layout.Size.y, rect.Max.y - rect.Min.y);
			}
		});

		DrawComponent<UILayoutGroupComponent>("UI Layout Group", entity, removeLayoutGroup, [&](UILayoutGroupComponent& group)
		{
			DrawPropertyRow("Direction", [&]()
			{
				const char* directions[] = { "Vertical", "Horizontal" };
				int direction = (int)group.Direction;
				if (ImGui::Combo("##GroupDirection", &direction, directions, 2))
				{
					group.Direction = (UILayoutDirection)direction;
				}
			});
			DrawPropertyRow("Spacing", [&]() { InputFloatField("##GroupSpacing", group.Spacing, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Padding L", [&]() { InputFloatField("##GroupPadLeft", group.Padding.x, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Padding B", [&]() { InputFloatField("##GroupPadBottom", group.Padding.y, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Padding R", [&]() { InputFloatField("##GroupPadRight", group.Padding.z, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Padding T", [&]() { InputFloatField("##GroupPadTop", group.Padding.w, "%.0f px", 0.0f, 4096.0f); });
			DrawPropertyRow("Alignment", [&]()
			{
				const char* alignments[] = { "Start (top / left)", "Center", "End (bottom / right)" };
				int alignment = (int)group.ChildAlignment;
				if (ImGui::Combo("##GroupAlignment", &alignment, alignments, 3))
				{
					group.ChildAlignment = (UILayoutAlignment)alignment;
				}
			});
			DrawPropertyRow("Stretch", [&]()
			{
				ImGui::Checkbox("##GroupStretch", &group.StretchCrossAxis);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Children take the content's width (vertical) or height (horizontal).");
				}
			});
			DrawPropertyRow("Fit Content", [&]()
			{
				ImGui::Checkbox("##GroupFit", &group.FitContent);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The group's own length along the stack follows its children; its pivot stays put.");
				}
			});
			ImGui::TextDisabled("Stacks the visible UI children; their own rects are left as authored.");
		});

		bool removeScroll = false;
		DrawComponent<UIScrollViewComponent>("UI Scroll View", entity, removeScroll, [&](UIScrollViewComponent& scroll)
		{
			DrawPropertyRow("Scroll", [&]() { InputFloatField("##ScrollPosition", scroll.ScrollPosition, "%.0f px", 0.0f, 100000.0f); });
			DrawPropertyRow("Follow Focus", [&]()
			{
				ImGui::Checkbox("##ScrollFollowFocus", &scroll.FollowFocus);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Scrolls so the focused button inside it is always whole in view.");
				}
			});
			DrawPropertyRow("Scrollbar", [&]()
			{
				// Any element of the canvas outside this view: the thumb, in its parent (the track).
				Scene& scene = *entity.GetScene();
				bool nested = false;
				const Entity canvas = UI::FindCanvas(entity, nested);
				Entity current = (uint64_t)scroll.Scrollbar != 0 ? scene.GetEntityByUUID(scroll.Scrollbar) : Entity{};
				const std::string preview = (bool)current ? current.GetName() : ((uint64_t)scroll.Scrollbar != 0 ? std::string("(missing)") : std::string("(none)"));
				if (ImGui::BeginCombo("##ScrollScrollbar", preview.c_str()))
				{
					if (ImGui::Selectable("(none)", (uint64_t)scroll.Scrollbar == 0))
					{
						scroll.Scrollbar = 0;
					}
					auto rects = scene.m_Registry.view<UIRectComponent>();
					for (entt::entity handle : rects)
					{
						Entity other{ handle, &scene };
						bool otherNested = false;
						if (other == entity || other.HasComponent<IDComponent>() == false || UI::FindCanvas(other, otherNested) != canvas
							|| scene.IsAncestorOf(entity, other))
						{
							continue;
						}
						ImGui::PushID((int)(uint32_t)handle);
						if (ImGui::Selectable(other.GetName().c_str(), other == current))
						{
							scroll.Scrollbar = other.GetUUID();
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
			});
			ImGui::TextDisabled("Moves its children up by Scroll and cuts everything below it to its rect.");
		});

		bool removeFocusVisibility = false;
		DrawComponent<UIFocusVisibilityComponent>("UI Focus Visibility", entity, removeFocusVisibility, [&](UIFocusVisibilityComponent& visibility)
		{
			DrawPropertyRow("Show", [&]()
			{
				const char* modes[] = { "While focused", "While not focused" };
				int mode = (int)visibility.Show;
				if (ImGui::Combo("##FocusVisibilityShow", &mode, modes, 2))
				{
					visibility.Show = (UIFocusVisibility)mode;
				}
			});
			ImGui::TextDisabled("By the state of its button (itself or the nearest one above it): a cursor, a highlight.");
		});
		if (removeScroll)
		{
			entity.RemoveComponent<UIScrollViewComponent>();
		}
		if (removeFocusVisibility)
		{
			entity.RemoveComponent<UIFocusVisibilityComponent>();
		}

		DrawComponent<UIButtonComponent>("UI Button", entity, removeButton, [&](UIButtonComponent& button)
		{
			DrawPropertyRow("Interactable", [&]() { ImGui::Checkbox("##ButtonInteractable", &button.Interactable); });
			DrawPropertyRow("Event", [&]() { InputTextField("##ButtonEvent", button.Event); });
			DrawPropertyRow("Normal", [&]() { ImGui::ColorEdit4("##ButtonNormal", glm::value_ptr(button.NormalTint)); });
			DrawPropertyRow("Focused", [&]() { ImGui::ColorEdit4("##ButtonFocused", glm::value_ptr(button.FocusedTint)); });
			DrawPropertyRow("Pressed", [&]() { ImGui::ColorEdit4("##ButtonPressed", glm::value_ptr(button.PressedTint)); });
			DrawPropertyRow("Disabled", [&]() { ImGui::ColorEdit4("##ButtonDisabled", glm::value_ptr(button.DisabledTint)); });
			DrawPropertyRow("Navigation", [&]()
			{
				const char* modes[] = { "Auto", "Explicit", "None" };
				int mode = (int)button.Navigation;
				if (ImGui::Combo("##ButtonNavigation", &mode, modes, 3))
				{
					button.Navigation = (UINavigation)mode;
				}
			});
			if (button.Navigation == UINavigation::Explicit)
			{
				DrawPropertyRow("Up", [&]() { DrawButtonReferenceField("##ButtonUp", entity, button.Up); });
				DrawPropertyRow("Down", [&]() { DrawButtonReferenceField("##ButtonDown", entity, button.Down); });
				DrawPropertyRow("Left", [&]() { DrawButtonReferenceField("##ButtonLeft", entity, button.Left); });
				DrawPropertyRow("Right", [&]() { DrawButtonReferenceField("##ButtonRight", entity, button.Right); });
			}

			Scene& scene = *entity.GetScene();
			if (const UIScene* ui = scene.GetUIScene())
			{
				const char* states[] = { "Normal", "Focused", "Pressed", "Disabled" };
				ImGui::TextDisabled("Now: %s", states[(int)ui->GetState(scene, entity)]);
			}
			else
			{
				ImGui::TextDisabled("Arrows / WASD move the focus, Enter / Space / Z press, Esc / Backspace / X cancel.");
			}
		});
	}


	void InspectorWindow::DrawAddComponentButton(Entity entity)
	{
		const float width = ImGui::GetContentRegionAvail().x;

		if (ImGui::Button("Add Component", { width, 0.0f }))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			const bool hasTransform = entity.HasComponent<TransformComponent>();
			const bool hasSprite = entity.HasComponent<SpriteRendererComponent>();
			const bool hasCamera = entity.HasComponent<CameraComponent>();
			const bool hasAnimator = entity.HasComponent<SpriteAnimatorComponent>();
			const bool hasLight = entity.HasComponent<Light2DComponent>();
			const bool hasShadow = entity.HasComponent<Shadow2DComponent>();
			const bool hasRigidbody = entity.HasComponent<Rigidbody2DComponent>();
			const bool hasBoxCollider = entity.HasComponent<BoxCollider2DComponent>();
			const bool hasCircleCollider = entity.HasComponent<CircleCollider2DComponent>();
			const bool hasEmitter = entity.HasComponent<ParticleEmitter2DComponent>();

			if (ImGui::MenuItem("Transform", nullptr, false, hasTransform == false))
				entity.AddComponent<TransformComponent>();

			if (ImGui::MenuItem("Sprite Renderer", nullptr, false, hasSprite == false))
			{
				entity.AddComponent<SpriteRendererComponent>();

				// A sprite needs a transform to be drawn (SceneRenderer views both).
				if (entity.HasComponent<TransformComponent>() == false)
					entity.AddComponent<TransformComponent>();
			}

			if (ImGui::MenuItem("Camera", nullptr, false, hasCamera == false))
			{
				// Only the first camera in a scene is primary by default.
				const bool firstCamera = (bool)entity.GetScene()->GetPrimaryCameraEntity() == false;
				entity.AddComponent<CameraComponent>().Primary = firstCamera;

				if (entity.HasComponent<TransformComponent>() == false)
					entity.AddComponent<TransformComponent>();
			}

			if (ImGui::MenuItem("Sprite Animator", nullptr, false, hasAnimator == false))
			{
				entity.AddComponent<SpriteAnimatorComponent>();

				// It animates the entity's sprite, which needs a transform to be drawn.
				if (entity.HasComponent<SpriteRendererComponent>() == false)
				{
					entity.AddComponent<SpriteRendererComponent>();
				}
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Light 2D", nullptr, false, hasLight == false))
			{
				entity.AddComponent<Light2DComponent>();

				// A point light is placed by the entity's transform.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Directional Light", nullptr, false, entity.HasComponent<DirectionalLightComponent>() == false))
			{
				// Pointing down and a little forward, so a new sun lights the ground at once.
				entity.AddComponent<DirectionalLightComponent>();
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>().Rotation = glm::vec3(glm::radians(-35.0f), 0.0f, 0.0f);
				}
			}

			if (ImGui::MenuItem("Mesh", nullptr, false, entity.HasComponent<MeshComponent>() == false))
			{
				entity.AddComponent<MeshComponent>();

				// Placed by the entity's transform.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Shadow 2D", nullptr, false, hasShadow == false))
			{
				entity.AddComponent<Shadow2DComponent>();

				// The blob is placed by the entity's transform.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::MenuItem("Particle Emitter 2D", nullptr, false, hasEmitter == false))
			{
				entity.AddComponent<ParticleEmitter2DComponent>();

				// Particles are emitted at the entity's position.
				if (entity.HasComponent<TransformComponent>() == false)
				{
					entity.AddComponent<TransformComponent>();
				}
			}

			if (ImGui::BeginMenu("Physics 2D"))
			{
				if (ImGui::MenuItem("Rigidbody 2D", nullptr, false, hasRigidbody == false))
				{
					entity.AddComponent<Rigidbody2DComponent>();

					// A body is placed by the entity's transform.
					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				if (ImGui::MenuItem("Box Collider 2D", nullptr, false, hasBoxCollider == false))
				{
					entity.AddComponent<BoxCollider2DComponent>();

					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				if (ImGui::MenuItem("Circle Collider 2D", nullptr, false, hasCircleCollider == false))
				{
					entity.AddComponent<CircleCollider2DComponent>();

					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Physics 3D"))
			{
				// Every one is placed by the entity's transform.
				const auto addWithTransform = [&entity](auto add)
				{
					add();
					if (entity.HasComponent<TransformComponent>() == false)
					{
						entity.AddComponent<TransformComponent>();
					}
				};

				if (ImGui::MenuItem("Box Collider", nullptr, false, entity.HasComponent<BoxColliderComponent>() == false))
				{
					addWithTransform([&]() { entity.AddComponent<BoxColliderComponent>(); });
				}
				if (ImGui::MenuItem("Sphere Collider", nullptr, false, entity.HasComponent<SphereColliderComponent>() == false))
				{
					addWithTransform([&]() { entity.AddComponent<SphereColliderComponent>(); });
				}
				if (ImGui::MenuItem("Capsule Collider", nullptr, false, entity.HasComponent<CapsuleColliderComponent>() == false))
				{
					addWithTransform([&]() { entity.AddComponent<CapsuleColliderComponent>(); });
				}
				if (ImGui::MenuItem("Cylinder Collider", nullptr, false, entity.HasComponent<CylinderColliderComponent>() == false))
				{
					addWithTransform([&]() { entity.AddComponent<CylinderColliderComponent>(); });
				}
				if (ImGui::MenuItem("Mesh Collider", nullptr, false, entity.HasComponent<MeshColliderComponent>() == false && entity.HasComponent<MeshComponent>()))
				{
					entity.AddComponent<MeshColliderComponent>();
				}
				if (ImGui::MenuItem("Character Body", nullptr, false, entity.HasComponent<CharacterBodyComponent>() == false))
				{
					addWithTransform([&]() { entity.AddComponent<CharacterBodyComponent>(); });
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("UI"))
			{
				// Elements are placed by a rect: every visual gets one with it. A canvas needs none.
				const auto addWithRect = [&entity](auto add)
				{
					add();
					if (entity.HasComponent<UIRectComponent>() == false)
					{
						entity.AddComponent<UIRectComponent>();
					}
				};

				if (ImGui::MenuItem("Canvas", nullptr, false, entity.HasComponent<UICanvasComponent>() == false))
				{
					entity.AddComponent<UICanvasComponent>();
				}
				if (ImGui::MenuItem("Rect", nullptr, false, entity.HasComponent<UIRectComponent>() == false))
				{
					entity.AddComponent<UIRectComponent>();
				}
				if (ImGui::MenuItem("Image", nullptr, false, entity.HasComponent<UIImageComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UIImageComponent>(); });
				}
				if (ImGui::MenuItem("Text", nullptr, false, entity.HasComponent<UITextComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UITextComponent>(); });
				}
				if (ImGui::MenuItem("Button", nullptr, false, entity.HasComponent<UIButtonComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UIButtonComponent>(); });
				}
				if (ImGui::MenuItem("Layout Group", nullptr, false, entity.HasComponent<UILayoutGroupComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UILayoutGroupComponent>(); });
				}
				if (ImGui::MenuItem("Scroll View", nullptr, false, entity.HasComponent<UIScrollViewComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UIScrollViewComponent>(); });
				}
				if (ImGui::MenuItem("Focus Visibility", nullptr, false, entity.HasComponent<UIFocusVisibilityComponent>() == false))
				{
					addWithRect([&]() { entity.AddComponent<UIFocusVisibilityComponent>(); });
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Script"))
			{
				DrawScriptMenuItems(entity);
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
	}

	void InspectorWindow::DrawScriptMenuItems(Entity entity)
	{
		const std::vector<std::string> names = ScriptRegistry::GetNames();
		if (names.empty())
		{
			ImGui::TextDisabled("No scripts registered");
			ImGui::TextDisabled("Mark a ScriptableEntity class HY_SCRIPT() and build");
			return;
		}

		// A script runs once per entity, so ones already added are greyed out. The component is only
		// added when a script is picked.
		const bool hasScripts = entity.HasComponent<NativeScriptComponent>();
		for (const std::string& name : names)
		{
			const bool added = hasScripts && entity.GetComponent<NativeScriptComponent>().Has(name);
			if (ImGui::MenuItem(name.c_str(), nullptr, false, added == false))
			{
				if (hasScripts)
					entity.GetComponent<NativeScriptComponent>().Bind(name);
				else
					entity.AddComponent<NativeScriptComponent>().Bind(name);
				return;
			}
		}
	}

	void InspectorWindow::DrawAsset(const std::filesystem::path& asset)
	{
		if (Texture2D::IsSupportedFile(asset))
		{
			m_TextureImportPanel.OnReimported = OnTextureReimported;
			m_TextureImportPanel.OnOpenSpriteEditor = OnOpenSpriteEditor;
			m_TextureImportPanel.OnImguiRender(asset);
			return;
		}

		if (asset.extension() == SpriteAnimationSerializer::FileExtension)
		{
			DrawAnimationSetAsset(asset);
			return;
		}

		if (asset.extension() == MaterialSerializer::FileExtension)
		{
			DrawMaterialAsset(asset);
			return;
		}

		if (GetAssetTypeFromExtension(asset) == AssetType::Mesh)
		{
			DrawModelAsset(asset);
			return;
		}

		if (asset.extension() == RenderProfileSerializer::FileExtension)
		{
			DrawRenderProfileAsset(asset);
			return;
		}

		ImGui::TextUnformatted(asset.filename().u8string().c_str());
		ImGui::TextDisabled("Nothing to edit for this kind of file.");
	}

	void InspectorWindow::DrawAnimationSetAsset(const std::filesystem::path& path)
	{
		ImGui::TextUnformatted(path.filename().u8string().c_str());
		// Read again only when another file is selected or this one was saved (a file that fails to
		// load would otherwise log every frame).
		std::error_code error;
		const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path, error);
		if (path != m_InspectedAnimationPath || writeTime != m_InspectedAnimationTime)
		{
			m_InspectedAnimationPath = path;
			m_InspectedAnimationTime = writeTime;
			m_InspectedAnimationSet = SpriteAnimationSet::LoadFromFile(path);
		}
		const Ref<SpriteAnimationSet>& set = m_InspectedAnimationSet;
		if (set == nullptr)
		{
			ImGui::TextDisabled("This file or its sprite sheet can't be read (see the Console).");
		}
		else
		{
			ImGui::TextDisabled("Sheet: %s", set->GetTexture()->GetPath().filename().u8string().c_str());
			ImGui::Spacing();
			if (ImGui::BeginTable("##Clips", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Clip");
				ImGui::TableSetupColumn("Frames");
				ImGui::TableSetupColumn("Loop");
				ImGui::TableHeadersRow();
				const char* loopNames[] = { "Once", "Loop", "Ping Pong" };
				for (uint32_t i = 0; i < set->GetClipCount(); i++)
				{
					const SpriteAnimationClip& clip = set->GetClip(i);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s%s", clip.Name.c_str(), i == set->GetDefaultClip() ? " (default)" : "");
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%zu, %.2f s", clip.Frames.size(), clip.GetLength());
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(loopNames[(int)clip.Loop]);
				}
				ImGui::EndTable();
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("Open in Animation Window", { ImGui::GetContentRegionAvail().x, 0.0f }) && OnOpenAnimation)
		{
			OnOpenAnimation(path);
		}
	}

	void InspectorWindow::DrawPrefabButton(Entity entity)
	{
		const std::filesystem::path path = EditorPaths::GetPrefabPath(entity.GetName());

		if (ImGui::Button("Save as Prefab", { ImGui::GetContentRegionAvail().x, 0.0f }))
			PrefabSerializer::Save(entity, path);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", path.string().c_str());
	}
}
