#include "EditorSettingsWindow.h"
#include "../EditorPaths.h"

#include <algorithm>

namespace Hydro
{
	static constexpr float SettingsLabelWidth = 150.0f;

	// Label column + a widget, like the inspector rows.
	static void BeginSettingsRow(const char* label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);
	}

	static bool BeginSettingsTable(const char* id)
	{
		if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp) == false)
			return false;

		ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, SettingsLabelWidth);
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
		return true;
	}

	// Typed number, clamped once the field is left (not while typing).
	static bool FloatRow(const char* label, float& value, float min, float max, const char* format = "%.1f")
	{
		BeginSettingsRow(label);
		ImGui::PushID(label);
		bool changed = ImGui::InputFloat("##value", &value, 0.0f, 0.0f, format);
		if (ImGui::IsItemDeactivatedAfterEdit())
			value = std::clamp(value, min, max);
		ImGui::PopID();
		return changed;
	}

	static bool Vec2Row(const char* label, ImVec2& value, float min, float max)
	{
		BeginSettingsRow(label);
		ImGui::PushID(label);
		float values[2] = { value.x, value.y };
		bool changed = ImGui::InputFloat2("##value", values, "%.1f");
		if (changed)
			value = ImVec2(values[0], values[1]);
		if (ImGui::IsItemDeactivatedAfterEdit())
			value = ImVec2(std::clamp(value.x, min, max), std::clamp(value.y, min, max));
		ImGui::PopID();
		return changed;
	}

	void EditorSettingsWindow::OnImguiRender(EditorSettings& settings)
	{
		if (m_Open == false)
			return;

		ImGui::SetNextWindowSize(ImVec2(480.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Editor Settings", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		EditorStyle& style = settings.Style;
		bool changed = false;

		changed |= DrawThemeSection(style);
		changed |= DrawSizesSection(style);
		changed |= DrawColorsSection(style);

		if (changed)
		{
			style.Apply();
			m_Unsaved = true;
		}

		// ---- Save / revert -----------------------------------------------------
		ImGui::Separator();
		if (ImGui::Button("Save") && OnSave)
		{
			OnSave();
			m_Unsaved = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert") && OnRevert)
		{
			OnRevert();
			m_Unsaved = false;
		}
		ImGui::SameLine();
		if (m_Unsaved)
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Unsaved changes");
		else
			ImGui::TextDisabled("%s", EditorPaths::GetEditorSettingsPath().filename().string().c_str());

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", EditorPaths::GetEditorSettingsPath().string().c_str());

		ImGui::End();
	}

	bool EditorSettingsWindow::DrawThemeSection(EditorStyle& style)
	{
		bool changed = false;

		if (ImGui::CollapsingHeader("Theme", ImGuiTreeNodeFlags_DefaultOpen) == false)
			return false;

		if (BeginSettingsTable("##theme"))
		{
			BeginSettingsRow("Theme");
			if (ImGui::BeginCombo("##Theme", EditorStyle::GetThemeName(style.Theme)))
			{
				const EditorTheme themes[] = { EditorTheme::Hydro, EditorTheme::Dark, EditorTheme::Light, EditorTheme::Classic };
				for (EditorTheme theme : themes)
				{
					if (ImGui::Selectable(EditorStyle::GetThemeName(theme), theme == style.Theme))
					{
						// Picking a theme replaces colours and sizes, but keeps the font scale.
						const float fontScale = style.FontScale;
						style = EditorStyle::FromTheme(theme);
						style.FontScale = fontScale;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}

			changed |= FloatRow("Font scale", style.FontScale, 0.5f, 3.0f, "%.2f");

			ImGui::EndTable();
		}

		if (ImGui::Button("Reset to theme"))
		{
			const float fontScale = style.FontScale;
			style = EditorStyle::FromTheme(style.Theme);
			style.FontScale = fontScale;
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Undo colour and size edits: back to the %s theme.", EditorStyle::GetThemeName(style.Theme));

		ImGui::Spacing();
		return changed;
	}

	bool EditorSettingsWindow::DrawSizesSection(EditorStyle& style)
	{
		bool changed = false;

		if (ImGui::CollapsingHeader("Sizes") == false)
			return false;

		if (BeginSettingsTable("##sizes"))
		{
			changed |= FloatRow("Window rounding", style.WindowRounding, 0.0f, 12.0f);
			changed |= FloatRow("Child rounding", style.ChildRounding, 0.0f, 12.0f);
			changed |= FloatRow("Frame rounding", style.FrameRounding, 0.0f, 12.0f);
			changed |= FloatRow("Popup rounding", style.PopupRounding, 0.0f, 12.0f);
			changed |= FloatRow("Scrollbar rounding", style.ScrollbarRounding, 0.0f, 12.0f);
			changed |= FloatRow("Grab rounding", style.GrabRounding, 0.0f, 12.0f);
			changed |= FloatRow("Tab rounding", style.TabRounding, 0.0f, 12.0f);

			changed |= FloatRow("Window border", style.WindowBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Child border", style.ChildBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Frame border", style.FrameBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Popup border", style.PopupBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Tab border", style.TabBorderSize, 0.0f, 2.0f);

			changed |= Vec2Row("Window padding", style.WindowPadding, 0.0f, 32.0f);
			changed |= Vec2Row("Frame padding", style.FramePadding, 0.0f, 24.0f);
			changed |= Vec2Row("Item spacing", style.ItemSpacing, 0.0f, 24.0f);
			changed |= Vec2Row("Item inner spacing", style.ItemInnerSpacing, 0.0f, 24.0f);
			changed |= FloatRow("Indent", style.IndentSpacing, 0.0f, 40.0f);
			changed |= FloatRow("Scrollbar size", style.ScrollbarSize, 4.0f, 30.0f);
			changed |= FloatRow("Grab min size", style.GrabMinSize, 4.0f, 30.0f);

			ImGui::EndTable();
		}

		ImGui::Spacing();
		return changed;
	}

	bool EditorSettingsWindow::DrawColorsSection(EditorStyle& style)
	{
		bool changed = false;

		if (ImGui::CollapsingHeader("Colors") == false)
			return false;

		m_ColorFilter.Draw("Filter##colors", -1.0f);

		if (BeginSettingsTable("##colors"))
		{
			for (int i = 0; i < ImGuiCol_COUNT; i++)
			{
				const char* name = ImGui::GetStyleColorName(i);
				if (m_ColorFilter.PassFilter(name) == false)
					continue;

				BeginSettingsRow(name);
				ImGui::PushID(i);
				changed |= ImGui::ColorEdit4("##color", &style.Colors[i].x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		return changed;
	}
}
