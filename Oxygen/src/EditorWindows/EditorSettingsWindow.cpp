#include "EditorSettingsWindow.h"
#include "Utilities/EditorChrome.h"
#include "../EditorPaths.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	static constexpr float SettingsLabelWidth = 160.0f;
	static constexpr float PageListWidth = 150.0f;

	struct PageInfo
	{
		const char* Name;
		const char* Description;
	};

	// Indexed by EditorSettingsWindow::Page.
	static const PageInfo Pages[] =
	{
		{ "Appearance",    "Theme, accent colour and text size." },
		{ "Play Mode",     "How the editor shows that the game is running." },
		{ "Layout",        "Rounding, borders and spacing of the theme." },
		{ "Colors",        "Every ImGui colour of the theme." },
		{ "Editor Camera", "The camera of the Editor window and its transform gizmo." },
		{ "Console",       "What each log line shows." },
		{ "Startup",       "What opens when Oxygen starts, and autosave." },
	};

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

	// A small bold heading inside a page.
	static void SectionHeading(const char* text)
	{
		ImGui::Spacing();
		ImFont* bold = EditorChrome::GetBoldFont();
		if (bold != nullptr)
			ImGui::PushFont(bold);
		ImGui::TextUnformatted(text);
		if (bold != nullptr)
			ImGui::PopFont();
		ImGui::Separator();
	}

	static void HelpMarker(const char* text)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", text);
	}

	// Typed number, clamped once the field is left (not while typing).
	static bool FloatRow(const char* label, float& value, float min, float max, const char* format = "%.1f")
	{
		BeginSettingsRow(label);
		ImGui::PushID(label);
		bool changed = ImGui::InputFloat("##value", &value, 0.0f, 0.0f, format);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			value = std::clamp(value, min, max);
			changed = true;
		}
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
		{
			value = ImVec2(std::clamp(value.x, min, max), std::clamp(value.y, min, max));
			changed = true;
		}
		ImGui::PopID();
		return changed;
	}

	static bool CheckboxRow(const char* label, bool& value)
	{
		BeginSettingsRow(label);
		ImGui::PushID(label);
		const bool changed = ImGui::Checkbox("##value", &value);
		ImGui::PopID();
		return changed;
	}

	static bool ColorRow(const char* label, ImVec4& value, bool alpha = false)
	{
		BeginSettingsRow(label);
		ImGui::PushID(label);
		const bool changed = alpha
			? ImGui::ColorEdit4("##value", &value.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)
			: ImGui::ColorEdit3("##value", &value.x);
		ImGui::PopID();
		return changed;
	}

	static bool SameColor(const ImVec4& a, const ImVec4& b)
	{
		const float epsilon = 0.002f;
		return std::abs(a.x - b.x) < epsilon && std::abs(a.y - b.y) < epsilon && std::abs(a.z - b.z) < epsilon;
	}

	void EditorSettingsWindow::OnImguiRender(EditorSettings& settings)
	{
		if (m_Open == false)
			return;

		ImGui::SetNextWindowSize(ImVec2(640.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Editor Settings", &m_Open) == false)
		{
			ImGui::End();
			return;
		}

		// Page list | page, above a footer with Save / Revert.
		const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + 1.0f;
		const float bodyHeight = std::max(ImGui::GetContentRegionAvail().y - footerHeight, 50.0f);

		ImGui::BeginChild("##pages", ImVec2(PageListWidth, bodyHeight), true);
		DrawPageList();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##page", ImVec2(0.0f, bodyHeight), false);
		if (DrawPage(settings))
			m_Unsaved = true;
		ImGui::EndChild();

		DrawFooter(settings);

		ImGui::End();
	}

	bool EditorSettingsWindow::DrawPageList()
	{
		bool changed = false;
		for (int i = 0; i < (int)std::size(Pages); i++)
		{
			if (ImGui::Selectable(Pages[i].Name, (int)m_Page == i))
			{
				m_Page = (Page)i;
				changed = true;
			}
		}
		return changed;
	}

	bool EditorSettingsWindow::DrawPage(EditorSettings& settings)
	{
		const PageInfo& info = Pages[(int)m_Page];

		ImFont* bold = EditorChrome::GetBoldFont();
		if (bold != nullptr)
			ImGui::PushFont(bold);
		ImGui::TextUnformatted(info.Name);
		if (bold != nullptr)
			ImGui::PopFont();
		ImGui::TextDisabled("%s", info.Description);
		ImGui::Spacing();

		bool styleChanged = false;
		bool changed = false;

		switch (m_Page)
		{
		case Page::Appearance:   styleChanged = DrawAppearancePage(settings); break;
		case Page::PlayMode:     changed = DrawPlayModePage(settings); break;
		case Page::Layout:       styleChanged = DrawLayoutPage(settings.Style); break;
		case Page::Colors:       styleChanged = DrawColorsPage(settings.Style); break;
		case Page::EditorCamera: changed = DrawEditorCameraPage(settings); break;
		case Page::Console:      changed = DrawConsolePage(settings.Console); break;
		case Page::Startup:      changed = DrawStartupPage(settings); break;
		}

		// The style is copied into ImGui; everything else the editor reads every frame.
		if (styleChanged)
			settings.Style.Apply();

		return changed || styleChanged;
	}

	void EditorSettingsWindow::DrawFooter(EditorSettings& settings)
	{
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
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reload the saved settings.");

		ImGui::SameLine();
		if (ImGui::Button("Defaults"))
		{
			// Everything back to the defaults, except what the editor has open.
			const EditorSession session = settings.Session;
			settings = EditorSettings();
			settings.Session = session;
			settings.Style.Apply();
			m_Unsaved = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Every setting back to its default. Save to keep it.");

		ImGui::SameLine();
		if (m_Unsaved)
			ImGui::TextColored(settings.Style.GetHighlightColor(), "Unsaved changes");
		else
			ImGui::TextDisabled("%s", EditorPaths::GetEditorSettingsPath().filename().string().c_str());

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", EditorPaths::GetEditorSettingsPath().string().c_str());
	}

	bool EditorSettingsWindow::DrawAppearancePage(EditorSettings& settings)
	{
		EditorStyle& style = settings.Style;
		bool changed = false;

		SectionHeading("Theme");
		if (BeginSettingsTable("##theme"))
		{
			BeginSettingsRow("Theme");
			if (ImGui::BeginCombo("##Theme", EditorStyle::GetThemeName(style.Theme)))
			{
				for (EditorTheme theme : EditorStyle::GetThemes())
				{
					if (ImGui::Selectable(EditorStyle::GetThemeName(theme), theme == style.Theme))
					{
						// Picking a theme replaces colours and sizes, but keeps the accent and font scale.
						const float fontScale = style.FontScale;
						style = EditorStyle::FromTheme(theme, style.Accent);
						style.FontScale = fontScale;
						changed = true;
					}

					if (theme == EditorTheme::Glacier)
						ImGui::Separator(); // the Hydro themes above, ImGui's presets below
				}
				ImGui::EndCombo();
			}

			ImGui::EndTable();
		}

		// Accent: preset swatches, then a free colour.
		const bool usesAccent = EditorStyle::UsesAccent(style.Theme);
		ImGui::BeginDisabled(usesAccent == false);

		SectionHeading("Accent");
		const float swatchSize = ImGui::GetFrameHeight() * 1.25f;
		for (const EditorAccentPreset& preset : EditorStyle::GetAccentPresets())
		{
			ImGui::PushID(preset.Name);
			const bool selected = SameColor(style.Accent, preset.Color);
			if (ImGui::ColorButton(preset.Name, preset.Color, ImGuiColorEditFlags_NoTooltip, ImVec2(swatchSize, swatchSize)))
			{
				style.Accent = preset.Color;
				style.RecolorFromAccent();
				changed = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", preset.Name);
			if (selected)
				EditorChrome::OutlineLastItem(style.Colors[ImGuiCol_Text], 2.0f);
			ImGui::PopID();
			ImGui::SameLine();
		}

		ImGui::TextDisabled("|");
		ImGui::SameLine();
		if (ImGui::ColorEdit3("Custom##accent", &style.Accent.x, ImGuiColorEditFlags_NoInputs))
		{
			style.RecolorFromAccent();
			changed = true;
		}

		ImGui::EndDisabled();

		if (usesAccent)
			ImGui::TextDisabled("Changing the accent recolours the theme (colour edits are replaced).");
		else
			ImGui::TextDisabled("ImGui's presets have fixed colours; pick Hydro, Abyss or Glacier for an accent.");

		SectionHeading("Text");
		if (BeginSettingsTable("##text"))
		{
			changed |= FloatRow("Scale", style.FontScale, 0.5f, 3.0f, "%.2f");

			// Font size changes need a restart; it doesn't touch the live style.
			BeginSettingsRow("Font size (px)");
			ImGui::PushID("FontSize");
			if (ImGui::InputFloat("##value", &settings.FontSize, 0.0f, 0.0f, "%.0f"))
				m_Unsaved = true;
			if (ImGui::IsItemDeactivatedAfterEdit())
				settings.FontSize = std::clamp(settings.FontSize, 10.0f, 32.0f);
			ImGui::PopID();

			ImGui::EndTable();
		}
		ImGui::TextDisabled("Font size applies after restarting Oxygen; Scale applies now (softer text).");

		ImGui::Spacing();
		if (ImGui::Button("Reset to theme"))
		{
			const float fontScale = style.FontScale;
			style = EditorStyle::FromTheme(style.Theme, style.Accent);
			style.FontScale = fontScale;
			changed = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Undo colour and size edits: back to the %s theme.", EditorStyle::GetThemeName(style.Theme));

		return changed;
	}

	bool EditorSettingsWindow::DrawPlayModePage(EditorSettings& settings)
	{
		PlayModeSettings& playMode = settings.PlayMode;
		bool changed = false;

		if (BeginSettingsTable("##playmode"))
		{
			changed |= CheckboxRow("Tint the editor", playMode.Tint);

			ImGui::BeginDisabled(playMode.Tint == false);
			changed |= ColorRow("Tint colour", playMode.TintColor);
			changed |= FloatRow("Strength", playMode.TintStrength, 0.0f, 1.0f, "%.2f");
			ImGui::EndDisabled();

			changed |= CheckboxRow("Full screen (F11) covers the monitor", playMode.FullscreenCoversMonitor);
			changed |= CheckboxRow("Leave full screen on Stop", playMode.LeaveFullscreenOnStop);

			ImGui::EndTable();
		}

		// Preview: a window background with and without the tint.
		ImGui::Spacing();
		const ImVec4 window = settings.Style.Colors[ImGuiCol_WindowBg];
		const float t = playMode.Tint ? playMode.TintStrength : 0.0f;
		const ImVec4 tinted(window.x + (playMode.TintColor.x - window.x) * t, window.y + (playMode.TintColor.y - window.y) * t,
			window.z + (playMode.TintColor.z - window.z) * t, 1.0f);

		const ImVec2 swatch(120.0f, ImGui::GetFrameHeight() * 1.5f);
		ImGui::BeginGroup();
		ImGui::ColorButton("Editing", window, ImGuiColorEditFlags_NoTooltip, swatch);
		ImGui::TextDisabled("Editing");
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::ColorButton("Playing", tinted, ImGuiColorEditFlags_NoTooltip, swatch);
		ImGui::TextDisabled("Playing");
		ImGui::EndGroup();

		ImGui::Spacing();
		ImGui::TextWrapped("While the game runs, windows, tabs and the menu bar take on the tint, the waterline under the menu bar "
			"turns the tint colour and the Game window gets an outline: changes made now are thrown away on Stop.");

		return changed;
	}

	bool EditorSettingsWindow::DrawLayoutPage(EditorStyle& style)
	{
		bool changed = false;

		SectionHeading("Rounding");
		if (BeginSettingsTable("##rounding"))
		{
			changed |= FloatRow("Window", style.WindowRounding, 0.0f, 12.0f);
			changed |= FloatRow("Child", style.ChildRounding, 0.0f, 12.0f);
			changed |= FloatRow("Frame", style.FrameRounding, 0.0f, 12.0f);
			changed |= FloatRow("Popup", style.PopupRounding, 0.0f, 12.0f);
			changed |= FloatRow("Scrollbar", style.ScrollbarRounding, 0.0f, 12.0f);
			changed |= FloatRow("Grab", style.GrabRounding, 0.0f, 12.0f);
			changed |= FloatRow("Tab", style.TabRounding, 0.0f, 12.0f);
			ImGui::EndTable();
		}
		ImGui::TextDisabled("Windows stay square while they can be dragged out of the main window.");

		SectionHeading("Borders");
		if (BeginSettingsTable("##borders"))
		{
			changed |= FloatRow("Window", style.WindowBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Child", style.ChildBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Frame", style.FrameBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Popup", style.PopupBorderSize, 0.0f, 2.0f);
			changed |= FloatRow("Tab", style.TabBorderSize, 0.0f, 2.0f);
			ImGui::EndTable();
		}

		SectionHeading("Spacing");
		if (BeginSettingsTable("##spacing"))
		{
			changed |= Vec2Row("Window padding", style.WindowPadding, 0.0f, 32.0f);
			changed |= Vec2Row("Frame padding", style.FramePadding, 0.0f, 24.0f);
			changed |= Vec2Row("Cell padding", style.CellPadding, 0.0f, 24.0f);
			changed |= Vec2Row("Item spacing", style.ItemSpacing, 0.0f, 24.0f);
			changed |= Vec2Row("Item inner spacing", style.ItemInnerSpacing, 0.0f, 24.0f);
			changed |= FloatRow("Indent", style.IndentSpacing, 0.0f, 40.0f);
			changed |= FloatRow("Scrollbar size", style.ScrollbarSize, 4.0f, 30.0f);
			changed |= FloatRow("Grab min size", style.GrabMinSize, 4.0f, 30.0f);
			ImGui::EndTable();
		}

		return changed;
	}

	bool EditorSettingsWindow::DrawColorsPage(EditorStyle& style)
	{
		bool changed = false;

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

	bool EditorSettingsWindow::DrawEditorCameraPage(EditorSettings& settings)
	{
		EditorCameraSettings& camera = settings.Camera;
		bool changed = false;

		if (BeginSettingsTable("##camera"))
		{
			changed |= CheckboxRow("Start in 2D", settings.CameraStartsIn2D);
			changed |= FloatRow("Field of view", camera.VerticalFOV, 5.0f, 150.0f, "%.0f");
			changed |= FloatRow("Near clip", camera.NearClip, 0.0001f, 100.0f, "%.4f");
			changed |= FloatRow("Far clip", camera.FarClip, 1.0f, 1000000.0f, "%.0f");
			changed |= FloatRow("Orbit speed", camera.OrbitSpeed, 0.05f, 10.0f, "%.2f");
			changed |= FloatRow("Pan speed", camera.PanSpeed, 0.05f, 10.0f, "%.2f");
			changed |= FloatRow("Zoom speed", camera.ZoomSpeed, 0.05f, 10.0f, "%.2f");
			changed |= FloatRow("Fly speed (m/s)", camera.FlySpeed, 0.1f, 200.0f, "%.1f");
			ImGui::EndTable();
		}

		ImGui::TextDisabled("Field of view and fly speed are used by the 3D view. The other speeds multiply the defaults (1 = default).");
		ImGui::TextDisabled("Controls: Alt+LMB orbit, MMB pan, Alt+RMB or the wheel to zoom.");
		ImGui::TextDisabled("3D: hold RMB to fly (mouse look, WASD, Q/E down/up, Shift faster, wheel = speed).");

		SectionHeading("Transform gizmo");
		if (BeginSettingsTable("##gizmo"))
		{
			changed |= FloatRow("Move snap (units)", settings.Gizmo.TranslateSnap, 0.001f, 1000.0f, "%.3f");
			changed |= FloatRow("Rotate snap (degrees)", settings.Gizmo.RotateSnap, 0.1f, 180.0f, "%.1f");
			changed |= FloatRow("Scale snap", settings.Gizmo.ScaleSnap, 0.001f, 100.0f, "%.3f");
			ImGui::EndTable();
		}
		ImGui::TextDisabled("W move, E rotate, R scale, Q hide the gizmo; T switches local / world. Hold Ctrl to snap.");

		ImGui::Spacing();
		if (ImGui::Button("Reset camera settings"))
		{
			camera = EditorCameraSettings();
			settings.CameraStartsIn2D = true;
			settings.Gizmo = GizmoSettings();
			changed = true;
		}

		return changed;
	}

	bool EditorSettingsWindow::DrawConsolePage(ConsoleSettings& console)
	{
		bool changed = false;

		if (BeginSettingsTable("##console"))
		{
			changed |= CheckboxRow("Auto-scroll", console.AutoScroll);
			changed |= CheckboxRow("Show time", console.ShowTimestamps);
			changed |= CheckboxRow("Show source", console.ShowSource);
			ImGui::EndTable();
		}
		ImGui::TextDisabled("Source is the logger: Hydro (engine) or APP (editor and game code).");

		return changed;
	}

	bool EditorSettingsWindow::DrawStartupPage(EditorSettings& settings)
	{
		bool changed = false;

		SectionHeading("Startup");
		if (BeginSettingsTable("##startup"))
		{
			changed |= CheckboxRow("Reopen last project", settings.Startup.ReopenLastProject);
			changed |= CheckboxRow("Reopen last scene", settings.Startup.ReopenLastScene);
			ImGui::EndTable();
		}

		const EditorSession& session = settings.Session;
		ImGui::TextDisabled("Last project: %s", session.LastProject.empty() ? "-" : session.LastProject.filename().string().c_str());
		if (ImGui::IsItemHovered() && session.LastProject.empty() == false)
			ImGui::SetTooltip("%s", session.LastProject.string().c_str());
		ImGui::TextDisabled("Last scene: %s", session.LastScene.empty() ? "-" : session.LastScene.filename().string().c_str());
		if (ImGui::IsItemHovered() && session.LastScene.empty() == false)
			ImGui::SetTooltip("%s", session.LastScene.string().c_str());

		SectionHeading("Autosave");
		if (BeginSettingsTable("##autosave"))
		{
			changed |= CheckboxRow("Save scene automatically", settings.Autosave.Enabled);
			ImGui::BeginDisabled(settings.Autosave.Enabled == false);
			changed |= FloatRow("Every (minutes)", settings.Autosave.IntervalMinutes, 0.5f, 120.0f);
			ImGui::EndDisabled();
			ImGui::EndTable();
		}
		ImGui::TextDisabled("Only while editing, and only a scene that has been saved once.");
		HelpMarker("Autosave writes the editor scene to its file, like File > Save Scene. Nothing is saved while playing.");

		return changed;
	}
}
