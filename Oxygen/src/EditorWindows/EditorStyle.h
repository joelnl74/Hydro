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
		Hydro = 0, // the editor's own dark theme (default)
		Dark,      // ImGui's dark preset
		Light,     // ImGui's light preset
		Classic    // ImGui's classic preset
	};

	// How the Oxygen editor's ImGui UI looks: every ImGui colour, the rounding/border/spacing sizes
	// and the font scale. Editor-only: the engine (Hydro library) never reads it. Saved as part of
	// EditorSettings (EditorSettings.yaml), edited in the Editor Settings window.
	struct EditorStyle
	{
		EditorTheme Theme = EditorTheme::Hydro;

		// Indexed by ImGuiCol_; saved by ImGui's colour names (ImGui::GetStyleColorName).
		std::array<ImVec4, ImGuiCol_COUNT> Colors{};

		// Rounding.
		float WindowRounding = 0.0f;
		float ChildRounding = 0.0f;
		float FrameRounding = 3.0f;
		float PopupRounding = 3.0f;
		float ScrollbarRounding = 6.0f;
		float GrabRounding = 3.0f;
		float TabRounding = 3.0f;

		// Borders.
		float WindowBorderSize = 1.0f;
		float ChildBorderSize = 1.0f;
		float FrameBorderSize = 0.0f;
		float PopupBorderSize = 1.0f;
		float TabBorderSize = 0.0f;

		// Spacing.
		ImVec2 WindowPadding = { 8.0f, 8.0f };
		ImVec2 FramePadding = { 6.0f, 4.0f };
		ImVec2 ItemSpacing = { 8.0f, 5.0f };
		ImVec2 ItemInnerSpacing = { 4.0f, 4.0f };
		float IndentSpacing = 18.0f;
		float ScrollbarSize = 14.0f;
		float GrabMinSize = 10.0f;

		// All text, relative to the loaded font (ImGuiIO::FontGlobalScale).
		float FontScale = 1.0f;

		// A complete style for a theme: its colours and sizes.
		static EditorStyle FromTheme(EditorTheme theme);
		static const char* GetThemeName(EditorTheme theme);
		// False (and `out` unchanged) when `name` isn't a theme name.
		static bool TryParseTheme(const std::string& name, EditorTheme& out);

		// Copies this style into ImGui (ImGui::GetStyle(), ImGuiIO::FontGlobalScale). Call when it
		// changed; the ImGui context must exist (after the ImGui layer is attached).
		void Apply() const;
	};
}
