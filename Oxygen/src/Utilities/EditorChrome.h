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

	// The editor's decorations over the ImGui theme: the brand, the menu bar's waterline, icon buttons and the
	// play-mode tint. Colours come from the caller, so it follows the theme.
	class EditorChrome
	{
	public:
		// Fonts loaded by OxygenEditorApplication; nullptr if the font file was missing.
		static ImFont* GetBoldFont();

		// The droplet and the editor's name, at the start of the current menu bar.
		static void DrawBrand(const ImVec4& accent);
		// A thin line along the bottom of the current menu bar, strongest on the left. Call between BeginMenuBar and EndMenuBar.
		static void DrawMenuBarWaterline(const ImVec4& color);

		// A square button with a drawn icon. `highlighted` fills it with `highlightColor`.
		static bool IconButton(const char* id, ToolbarIcon icon, float size, bool highlighted, const ImVec4& highlightColor,
			const char* tooltip);
		// A rectangle outline around the last item (e.g. the Game window image).
		static void OutlineLastItem(const ImVec4& color, float thickness);

		// Tints window, title, menu-bar and tab colours towards the play-mode tint until PopPlayModeTint. Always pair them.
		static void PushPlayModeTint(const PlayModeSettings& settings, bool playing);
		static void PopPlayModeTint();

	private:
		static int s_PushedTintColors;
	};
}
