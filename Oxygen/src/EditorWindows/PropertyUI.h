#pragma once
#include <imgui/imgui.h>

#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/Texture2D.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cfloat>
#include <cstdint>

namespace Hydro
{
	// Small widgets shared by the asset editors (texture import, Sprite Editor, Animation window),
	// so their rows look like the inspector's.
	class PropertyUI
	{
	public:
		static constexpr float LabelWidth = 110.0f;

		// A two-column row: label, then whatever `widget` draws (full width).
		template<typename WidgetFunction>
		static void Row(const char* label, WidgetFunction widget)
		{
			ImGui::PushID(label);
			if (ImGui::BeginTable("##row", 2, ImGuiTableFlags_None))
			{
				ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
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

		// A number you type into; clamped to [min, max] once you leave the field. True when it changed.
		static bool InputFloat(const char* id, float& value, const char* format = "%.3f", float min = -FLT_MAX, float max = FLT_MAX)
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

		// Two unsigned numbers side by side (sizes, offsets in pixels). True when one changed.
		static bool InputUVec2(const char* id, glm::uvec2& value, uint32_t min = 0, uint32_t max = 65536)
		{
			int values[2] = { (int)value.x, (int)value.y };
			const bool changed = ImGui::InputInt2(id, values);
			if (changed)
			{
				value.x = (uint32_t)std::clamp(values[0], (int)min, (int)max);
				value.y = (uint32_t)std::clamp(values[1], (int)min, (int)max);
			}
			return changed;
		}

		static bool InputUInt(const char* id, uint32_t& value, uint32_t min = 0, uint32_t max = 65536)
		{
			int number = (int)value;
			const bool changed = ImGui::InputInt(id, &number);
			if (changed)
			{
				value = (uint32_t)std::clamp(number, (int)min, (int)max);
			}
			return changed;
		}

		// Draws sprite `index` of `texture` into a size x size box, keeping its aspect ratio
		// (letterboxed). Flipped horizontally when flipX.
		static void SpriteImage(const Ref<Texture2D>& texture, uint32_t index, float size, bool flipX = false)
		{
			const ImVec2 cursor = ImGui::GetCursorPos();
			ImGui::Dummy({ size, size });
			if (texture == nullptr)
			{
				return;
			}

			const SpriteRegion& sprite = texture->GetSprite(index);
			const float width = (float)std::max(sprite.Size.x, 1u);
			const float height = (float)std::max(sprite.Size.y, 1u);
			const float scale = size / std::max(width, height);
			const ImVec2 imageSize = { width * scale, height * scale };

			TextureRect rect = texture->GetSpriteUVRect(index);
			if (flipX)
			{
				std::swap(rect.Min.x, rect.Max.x);
			}

			const ImVec2 after = ImGui::GetCursorPos();
			ImGui::SetCursorPos({ cursor.x + (size - imageSize.x) * 0.5f, cursor.y + (size - imageSize.y) * 0.5f });
			ImGui::Image((ImTextureID)Renderer2D::GetImGuiTexture(texture), imageSize, { rect.Min.x, rect.Min.y }, { rect.Max.x, rect.Max.y });
			ImGui::SetCursorPos(after);
		}
	};
}
