#pragma once
#include <imgui/imgui.h>

#include <cstdint>

namespace Hydro
{
	struct PlayModeSettings;

	enum class ToolbarIcon : uint8_t
	{
		Play,
		Stop,
		Pause,
		Step
	};

	// The editor's own decorations, drawn on top of the ImGui theme: the Hydro droplet in the menu
	// bar, the accent "waterline" under it, icon buttons for the play controls and the play-mode tint.
	// Everything takes its colours from the caller (EditorStyle::GetHighlightColor, PlayModeSettings),
	// so it follows the theme.
	class EditorChrome
	{
	public:
		// Fonts loaded by OxygenEditorApplication; nullptr if the font file was missing.
		static ImFont* GetBoldFont();

		// The droplet and the editor's name, at the start of the current menu bar.
		static void DrawBrand(const ImVec4& accent);
		// A thin line along the bottom of the current window's menu bar, strongest on the left.
		// Call between BeginMenuBar and EndMenuBar.
		static void DrawMenuBarWaterline(const ImVec4& color);

		// A square button with a drawn icon. `highlighted` fills it with `highlightColor`.
		static bool IconButton(const char* id, ToolbarIcon icon, float size, bool highlighted, const ImVec4& highlightColor, const char* tooltip);

		// A rectangle outline around the last item (e.g. the Game window image).
		static void OutlineLastItem(const ImVec4& color, float thickness);

		// Blends the window, title, menu-bar and tab colours towards the play-mode tint for everything
		// drawn until PopPlayModeTint. Always pair them; it pushes nothing when the tint is off.
		static void PushPlayModeTint(const PlayModeSettings& settings, bool playing);
		static void PopPlayModeTint();

	private:
		static int s_PushedTintColors;
	};
}
