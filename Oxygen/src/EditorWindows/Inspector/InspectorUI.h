#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Scene/Components/Components.h>

#include <imgui/imgui.h>

#include <cfloat>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace Hydro
{
	class Texture2D;

	// The inspector's rows and fields, shared by the component drawers and the asset inspectors.
	class InspectorUI
	{
	public:
		static constexpr float LabelWidth = 100.0f;

		// A label column + one widget, matching the vector rows.
		template<typename WidgetFunction>
		static void Row(const char* label, WidgetFunction widget)
		{
			ImGui::PushID(label);
			if (ImGui::BeginTable("##property", 2, ImGuiTableFlags_None))
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

		// A number you type into. It updates as you type and is clamped to [min, max] once you leave the field, so a
		// half-typed number is never clamped from under you. True when the value changed.
		static bool InputFloat(const char* id, float& value, const char* format = "%.3f", float min = -FLT_MAX, float max = FLT_MAX);
		// One row: a label, then X/Y(/Z/W) fields. Clicking an axis button resets that axis; each component is clamped
		// to [min, max] when its field is left. True when a value changed.
		static bool DrawVector(const char* label, float* values, int count, float resetValue = 0.0f, float min = -FLT_MAX,
			float max = FLT_MAX);
		static bool DrawVec3(const char* label, glm::vec3& values, float resetValue = 0.0f);

		// ImGui::InputText on a std::string that grows as you type (what imgui_stdlib does): pass it with the string
		// as user data and ImGuiInputTextFlags_CallbackResize.
		static int StringResizeCallback(ImGuiInputTextCallbackData* data);
		static bool InputText(const char* id, std::string& text);

		// A texture field: pick one of the project's images or drop one from the Project window; X clears it.
		static void TextureField(const char* id, AssetHandle& textureHandle);
		// A font field (UI text): pick one of the project's .fnt fonts, or drop one from the Project window; X clears it.
		static void FontField(const char* id, AssetHandle& fontHandle);
		// For a sprite sheet (a Multiple texture): which of its sprites to draw.
		static void SpriteField(uint32_t& spriteIndex, const Texture2D& texture);
		// Which layer a 3D collider, character or terrain is on (PhysicsLayer).
		static void PhysicsLayerRow(PhysicsLayer& layer);
	};
}
