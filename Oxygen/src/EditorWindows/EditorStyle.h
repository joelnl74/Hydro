#pragma once
#include <imgui/imgui.h>

#include <array>
#include <cstdint>
#include <string>

namespace Hydro
{
	// The preset a style starts from. Editing a colour or size afterwards keeps the theme name
	// (it says where the style came from); "Reset to theme" goes back to the preset.
	enum class EditorTheme : uint8_t
	{
		Hydro = 0, // the editor's own theme (default): deep ocean blues, one accent colour
		Dark,      // ImGui's dark preset
		Light,     // ImGui's light preset
		Classic,   // ImGui's classic preset
		Abyss,     // Hydro, darker: near-black blues for dim rooms
		Glacier    // Hydro, light: ice-white panels
	};

	// A named accent colour offered in the Editor Settings window.
	struct EditorAccentPreset
	{
		const char* Name;
		ImVec4 Color;
	};

	// How the Oxygen editor's ImGui UI looks: every ImGui colour, the rounding/border/spacing sizes
	// and the font scale. Editor-only: the engine (Hydro library) never reads it. Saved as part of
	// EditorSettings (EditorSettings.yaml), edited in the Editor Settings window.
	//
	// The Hydro themes (Hydro, Abyss, Glacier) are built from one accent colour: picking another
	// accent recolours every highlight (selection, checkmarks, sliders, tabs, the menu-bar waterline).
	struct EditorStyle
	{
		EditorTheme Theme = EditorTheme::Hydro;
		ImVec4 Accent = GetDefaultAccent();

		// Indexed by ImGuiCol_; saved by ImGui's colour names (ImGui::GetStyleColorName).
		std::array<ImVec4, ImGuiCol_COUNT> Colors{};

		// Rounding. Windows stay square while ImGui viewports are enabled (OS windows can't be round).
		float WindowRounding = 0.0f;
		float ChildRounding = 4.0f;
		float FrameRounding = 4.0f;
		float PopupRounding = 6.0f;
		float ScrollbarRounding = 12.0f;
		float GrabRounding = 4.0f;
		float TabRounding = 5.0f;

		// Borders.
		float WindowBorderSize = 1.0f;
		float ChildBorderSize = 1.0f;
		float FrameBorderSize = 0.0f;
		float PopupBorderSize = 1.0f;
		float TabBorderSize = 0.0f;

		// Spacing.
		ImVec2 WindowPadding = { 10.0f, 8.0f };
		ImVec2 FramePadding = { 8.0f, 5.0f };
		ImVec2 CellPadding = { 6.0f, 3.0f };
		ImVec2 ItemSpacing = { 8.0f, 6.0f };
		ImVec2 ItemInnerSpacing = { 6.0f, 4.0f };
		float IndentSpacing = 16.0f;
		float ScrollbarSize = 11.0f;
		float GrabMinSize = 10.0f;

		// All text, relative to the loaded font (ImGuiIO::FontGlobalScale).
		float FontScale = 1.0f;

		// A complete style for a theme: its colours and sizes. `accent` is used by the Hydro themes.
		static EditorStyle FromTheme(EditorTheme theme, const ImVec4& accent = GetDefaultAccent());
		static const char* GetThemeName(EditorTheme theme);
		// False (and `out` unchanged) when `name` isn't a theme name.
		static bool TryParseTheme(const std::string& name, EditorTheme& out);
		// In the order the Editor Settings window lists them.
		static const std::array<EditorTheme, 6>& GetThemes();
		// Hydro, Abyss and Glacier are built from the accent colour; ImGui's presets aren't.
		static bool UsesAccent(EditorTheme theme);

		static ImVec4 GetDefaultAccent() { return ImVec4(0.13f, 0.76f, 0.85f, 1.0f); } // Aqua
		static const std::array<EditorAccentPreset, 6>& GetAccentPresets();

		// The colour the editor's own decorations use: the accent for the Hydro themes, the checkmark
		// colour for ImGui's presets.
		ImVec4 GetHighlightColor() const;

		// Rebuilds the colours from Theme and Accent (sizes are kept). Used when the accent changes.
		void RecolorFromAccent();

		// Copies this style into ImGui (ImGui::GetStyle(), ImGuiIO::FontGlobalScale). Call when it
		// changed; the ImGui context must exist (after the ImGui layer is attached).
		void Apply() const;
	};
}
