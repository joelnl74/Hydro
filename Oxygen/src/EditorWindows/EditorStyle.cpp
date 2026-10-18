#include "EditorStyle.h"

namespace Hydro
{
	static ImVec4 Rgb(int r, int g, int b, float a = 1.0f)
	{
		return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
	}

	// Linear blend: t = 0 gives a, t = 1 gives b. Alpha comes from a.
	static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
	{
		return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w);
	}

	static ImVec4 WithAlpha(const ImVec4& color, float alpha)
	{
		return ImVec4(color.x, color.y, color.z, alpha);
	}

	// The surfaces a Hydro theme is built on; the accent colour supplies every highlight.
	struct HydroSurfaces
	{
		ImVec4 Deep;         // title bars, menu bar, empty dock space, inactive tabs
		ImVec4 Window;       // window backgrounds
		ImVec4 Popup;        // menus, popups, tooltips
		ImVec4 Frame;        // input fields, checkboxes, sliders
		ImVec4 Raised;       // buttons, headers, table headers
		ImVec4 Border;
		ImVec4 Text;
		ImVec4 TextDisabled;
		bool Light;
	};

	// Hydro: deep ocean. Blue-slate surfaces that get darker towards the window edges.
	static HydroSurfaces OceanSurfaces()
	{
		return { Rgb(10, 15, 21), Rgb(16, 22, 30), Rgb(19, 27, 37), Rgb(25, 34, 46), Rgb(31, 42, 56),
			Rgb(35, 48, 65), Rgb(213, 225, 236), Rgb(93, 111, 132), false };
	}

	// Abyss: the same ocean, deeper. For dark rooms and OLED screens.
	static HydroSurfaces AbyssSurfaces()
	{
		return { Rgb(4, 7, 11), Rgb(9, 13, 19), Rgb(13, 19, 26), Rgb(18, 25, 34), Rgb(24, 33, 44),
			Rgb(26, 36, 48), Rgb(201, 214, 227), Rgb(83, 100, 119), false };
	}

	// Glacier: ice-white panels, blue-grey text.
	static HydroSurfaces GlacierSurfaces()
	{
		return { Rgb(213, 223, 232), Rgb(237, 242, 246), Rgb(249, 251, 252), Rgb(255, 255, 255), Rgb(225, 233, 240),
			Rgb(194, 207, 219), Rgb(23, 36, 51), Rgb(125, 141, 158), true };
	}

	static void BuildHydroColors(const HydroSurfaces& s, const ImVec4& accentColor, std::array<ImVec4, ImGuiCol_COUNT>& colors)
	{
		const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
		const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
		const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);

		const ImVec4 accent = WithAlpha(accentColor, 1.0f);
		// Brighter on dark themes, deeper on the light one, so hovered/active states stand out.
		const ImVec4 accentStrong = s.Light ? Mix(accent, black, 0.18f) : Mix(accent, white, 0.18f);

		colors[ImGuiCol_Text] = s.Text;
		colors[ImGuiCol_TextDisabled] = s.TextDisabled;
		colors[ImGuiCol_WindowBg] = s.Window;
		colors[ImGuiCol_ChildBg] = clear;
		colors[ImGuiCol_PopupBg] = WithAlpha(s.Popup, 0.98f);
		colors[ImGuiCol_Border] = s.Border;
		colors[ImGuiCol_BorderShadow] = clear;

		colors[ImGuiCol_FrameBg] = s.Frame;
		colors[ImGuiCol_FrameBgHovered] = Mix(s.Frame, accent, 0.14f);
		colors[ImGuiCol_FrameBgActive] = Mix(s.Frame, accent, 0.26f);

		colors[ImGuiCol_TitleBg] = s.Deep;
		colors[ImGuiCol_TitleBgActive] = Mix(s.Deep, accent, 0.10f);
		colors[ImGuiCol_TitleBgCollapsed] = WithAlpha(s.Deep, 0.75f);
		colors[ImGuiCol_MenuBarBg] = s.Deep;

		const ImVec4 grab = s.Light ? Mix(s.Border, s.Text, 0.15f) : Mix(s.Raised, s.Text, 0.12f);
		colors[ImGuiCol_ScrollbarBg] = WithAlpha(s.Deep, 0.35f);
		colors[ImGuiCol_ScrollbarGrab] = grab;
		colors[ImGuiCol_ScrollbarGrabHovered] = Mix(grab, accent, 0.45f);
		colors[ImGuiCol_ScrollbarGrabActive] = accent;

		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentStrong;

		colors[ImGuiCol_Button] = s.Raised;
		colors[ImGuiCol_ButtonHovered] = Mix(s.Raised, accent, 0.30f);
		colors[ImGuiCol_ButtonActive] = Mix(s.Raised, accent, 0.50f);

		// Selection (hierarchy, selectables) and collapsing headers (inspector components).
		colors[ImGuiCol_Header] = Mix(s.Raised, accent, 0.20f);
		colors[ImGuiCol_HeaderHovered] = Mix(s.Raised, accent, 0.34f);
		colors[ImGuiCol_HeaderActive] = Mix(s.Raised, accent, 0.48f);

		colors[ImGuiCol_Separator] = s.Border;
		colors[ImGuiCol_SeparatorHovered] = WithAlpha(accent, 0.70f);
		colors[ImGuiCol_SeparatorActive] = accent;

		colors[ImGuiCol_ResizeGrip] = WithAlpha(accent, 0.15f);
		colors[ImGuiCol_ResizeGripHovered] = WithAlpha(accent, 0.55f);
		colors[ImGuiCol_ResizeGripActive] = WithAlpha(accent, 0.90f);

		// Tabs sit on the deep colour; the selected one lifts towards the window and takes a hint of
		// the accent.
		colors[ImGuiCol_Tab] = s.Deep;
		colors[ImGuiCol_TabHovered] = Mix(s.Window, accent, 0.30f);
		colors[ImGuiCol_TabActive] = Mix(s.Frame, accent, 0.22f);
		colors[ImGuiCol_TabUnfocused] = s.Deep;
		colors[ImGuiCol_TabUnfocusedActive] = s.Window;

		colors[ImGuiCol_DockingPreview] = WithAlpha(accent, 0.55f);
		colors[ImGuiCol_DockingEmptyBg] = s.Deep;

		colors[ImGuiCol_PlotLines] = Mix(s.Text, accent, 0.35f);
		colors[ImGuiCol_PlotLinesHovered] = accentStrong;
		colors[ImGuiCol_PlotHistogram] = accent;
		colors[ImGuiCol_PlotHistogramHovered] = accentStrong;

		colors[ImGuiCol_TableHeaderBg] = s.Raised;
		colors[ImGuiCol_TableBorderStrong] = s.Border;
		colors[ImGuiCol_TableBorderLight] = Mix(s.Border, s.Window, 0.5f);
		colors[ImGuiCol_TableRowBg] = clear;
		colors[ImGuiCol_TableRowBgAlt] = s.Light ? WithAlpha(black, 0.03f) : WithAlpha(white, 0.025f);

		colors[ImGuiCol_TextSelectedBg] = WithAlpha(accent, 0.30f);
		colors[ImGuiCol_DragDropTarget] = accentStrong;
		colors[ImGuiCol_NavHighlight] = accent;
		colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(white, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = WithAlpha(s.Deep, 0.45f);
		// Modal popups dim the editor with the deep ocean colour rather than black.
		colors[ImGuiCol_ModalWindowDimBg] = WithAlpha(s.Deep, 0.65f);
	}

	static HydroSurfaces GetSurfaces(EditorTheme theme)
	{
		switch (theme)
		{
		case EditorTheme::Abyss:   return AbyssSurfaces();
		case EditorTheme::Glacier: return GlacierSurfaces();
		case EditorTheme::Hydro:
		case EditorTheme::Dark:
		case EditorTheme::Light:
		case EditorTheme::Classic: break;
		}
		return OceanSurfaces();
	}

	const std::array<EditorTheme, 6>& EditorStyle::GetThemes()
	{
		static const std::array<EditorTheme, 6> themes =
		{
			EditorTheme::Hydro, EditorTheme::Abyss, EditorTheme::Glacier,
			EditorTheme::Dark, EditorTheme::Light, EditorTheme::Classic
		};
		return themes;
	}

	const std::array<EditorAccentPreset, 6>& EditorStyle::GetAccentPresets()
	{
		static const std::array<EditorAccentPreset, 6> presets =
		{ {
			{ "Aqua",   ImVec4(0.13f, 0.76f, 0.85f, 1.0f) },
			{ "Lagoon", ImVec4(0.17f, 0.82f, 0.63f, 1.0f) },
			{ "Tide",   ImVec4(0.30f, 0.55f, 1.00f, 1.0f) },
			{ "Orchid", ImVec4(0.73f, 0.51f, 1.00f, 1.0f) },
			{ "Coral",  ImVec4(1.00f, 0.44f, 0.38f, 1.0f) },
			{ "Amber",  ImVec4(0.96f, 0.72f, 0.24f, 1.0f) },
		} };
		return presets;
	}

	bool EditorStyle::UsesAccent(EditorTheme theme)
	{
		return theme == EditorTheme::Hydro || theme == EditorTheme::Abyss || theme == EditorTheme::Glacier;
	}

	EditorStyle EditorStyle::FromTheme(EditorTheme theme, const ImVec4& accent)
	{
		EditorStyle style;
		style.Theme = theme;
		style.Accent = accent;

		if (UsesAccent(theme))
		{
			// The default sizes in EditorStyle are the Hydro sizes.
			const HydroSurfaces surfaces = GetSurfaces(theme);
			BuildHydroColors(surfaces, accent, style.Colors);

			// On the light theme, fields need an outline to stand out from the panel.
			if (surfaces.Light)
				style.FrameBorderSize = 1.0f;

			return style;
		}

		// ImGui's presets: their colours and ImGui's own sizes.
		ImGuiStyle imguiStyle;
		switch (theme)
		{
		case EditorTheme::Light:   ImGui::StyleColorsLight(&imguiStyle); break;
		case EditorTheme::Classic: ImGui::StyleColorsClassic(&imguiStyle); break;
		case EditorTheme::Dark:
		case EditorTheme::Hydro:
		case EditorTheme::Abyss:
		case EditorTheme::Glacier: ImGui::StyleColorsDark(&imguiStyle); break;
		}

		for (int i = 0; i < ImGuiCol_COUNT; i++)
			style.Colors[i] = imguiStyle.Colors[i];

		style.WindowRounding = imguiStyle.WindowRounding;
		style.ChildRounding = imguiStyle.ChildRounding;
		style.FrameRounding = imguiStyle.FrameRounding;
		style.PopupRounding = imguiStyle.PopupRounding;
		style.ScrollbarRounding = imguiStyle.ScrollbarRounding;
		style.GrabRounding = imguiStyle.GrabRounding;
		style.TabRounding = imguiStyle.TabRounding;
		style.WindowBorderSize = imguiStyle.WindowBorderSize;
		style.ChildBorderSize = imguiStyle.ChildBorderSize;
		style.FrameBorderSize = theme == EditorTheme::Light ? 1.0f : imguiStyle.FrameBorderSize;
		style.PopupBorderSize = imguiStyle.PopupBorderSize;
		style.TabBorderSize = imguiStyle.TabBorderSize;
		style.WindowPadding = imguiStyle.WindowPadding;
		style.FramePadding = imguiStyle.FramePadding;
		style.CellPadding = imguiStyle.CellPadding;
		style.ItemSpacing = imguiStyle.ItemSpacing;
		style.ItemInnerSpacing = imguiStyle.ItemInnerSpacing;
		style.IndentSpacing = imguiStyle.IndentSpacing;
		style.ScrollbarSize = imguiStyle.ScrollbarSize;
		style.GrabMinSize = imguiStyle.GrabMinSize;

		return style;
	}

	void EditorStyle::RecolorFromAccent()
	{
		if (UsesAccent(Theme))
			BuildHydroColors(GetSurfaces(Theme), Accent, Colors);
	}

	ImVec4 EditorStyle::GetHighlightColor() const
	{
		return UsesAccent(Theme) ? WithAlpha(Accent, 1.0f) : Colors[ImGuiCol_CheckMark];
	}

	const char* EditorStyle::GetThemeName(EditorTheme theme)
	{
		switch (theme)
		{
		case EditorTheme::Hydro:   return "Hydro";
		case EditorTheme::Abyss:   return "Abyss";
		case EditorTheme::Glacier: return "Glacier";
		case EditorTheme::Dark:    return "Dark";
		case EditorTheme::Light:   return "Light";
		case EditorTheme::Classic: return "Classic";
		}
		return "Hydro";
	}

	bool EditorStyle::TryParseTheme(const std::string& name, EditorTheme& out)
	{
		for (EditorTheme theme : GetThemes())
		{
			if (name == GetThemeName(theme))
			{
				out = theme;
				return true;
			}
		}
		return false;
	}

	void EditorStyle::Apply() const
	{
		ImGuiStyle& style = ImGui::GetStyle();

		for (int i = 0; i < ImGuiCol_COUNT; i++)
			style.Colors[i] = Colors[i];

		style.WindowRounding = WindowRounding;
		style.ChildRounding = ChildRounding;
		style.FrameRounding = FrameRounding;
		style.PopupRounding = PopupRounding;
		style.ScrollbarRounding = ScrollbarRounding;
		style.GrabRounding = GrabRounding;
		style.TabRounding = TabRounding;

		style.WindowBorderSize = WindowBorderSize;
		style.ChildBorderSize = ChildBorderSize;
		style.FrameBorderSize = FrameBorderSize;
		style.PopupBorderSize = PopupBorderSize;
		style.TabBorderSize = TabBorderSize;

		style.WindowPadding = WindowPadding;
		style.FramePadding = FramePadding;
		style.CellPadding = CellPadding;
		style.ItemSpacing = ItemSpacing;
		style.ItemInnerSpacing = ItemInnerSpacing;
		style.IndentSpacing = IndentSpacing;
		style.ScrollbarSize = ScrollbarSize;
		style.GrabMinSize = GrabMinSize;

		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = FontScale;

		// Windows dragged out of the main window become OS windows: they must be square and opaque
		// (the same rule the engine's ImGui layer applies).
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
	}
}
