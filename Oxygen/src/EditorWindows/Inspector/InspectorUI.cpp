#include "InspectorUI.h"
#include "Utilities/AssetPicker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/Font.h>
#include <Hydro/Renderer/Texture2D.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>

namespace Hydro
{
	bool InspectorUI::InputFloat(const char* id, float& value, const char* format, float min, float max)
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

	bool InspectorUI::DrawVector(const char* label, float* values, int count, float resetValue, float min, float max)
	{
		bool changed = false;

		ImGui::PushID(label);

		if (ImGui::BeginTable("##vector", 2, ImGuiTableFlags_None))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
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
				changed |= InputFloat("##value", values[i], "%.3f", min, max);
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

	bool InspectorUI::DrawVec3(const char* label, glm::vec3& values, float resetValue)
	{
		return DrawVector(label, glm::value_ptr(values), 3, resetValue);
	}

	int InspectorUI::StringResizeCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			std::string* text = static_cast<std::string*>(data->UserData);
			text->resize((size_t)data->BufTextLen);
			data->Buf = text->data();
		}
		return 0;
	}

	bool InspectorUI::InputText(const char* id, std::string& text)
	{
		return ImGui::InputText(id, text.data(), text.capacity() + 1, ImGuiInputTextFlags_CallbackResize, StringResizeCallback, &text);
	}

	void InspectorUI::TextureField(const char* id, AssetHandle& textureHandle)
	{
		const Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
		const std::string detail = texture != nullptr
			? std::to_string(texture->GetWidth()) + " x " + std::to_string(texture->GetHeight()) + " px"
			: (static_cast<uint64_t>(textureHandle) != 0 ? "The texture is missing or couldn't be loaded (see the console)."
				: std::string());

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

	void InspectorUI::FontField(const char* id, AssetHandle& fontHandle)
	{
		const Ref<Font> font = AssetManager::GetAsset<Font>(fontHandle);
		const std::string detail = font != nullptr
			? font->GetData().Face + ", " + std::to_string((int)font->GetData().Size) + " px, " + std::to_string(font->GetData().Glyphs.size()) + " characters"
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

	void InspectorUI::SpriteField(uint32_t& spriteIndex, const Texture2D& texture)
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

	void InspectorUI::PhysicsLayerRow(PhysicsLayer& layer)
	{
		const char* names[] = { "World", "Player", "Npc", "Trigger" };
		int index = (int)layer;
		Row("Layer", [&]()
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
}
