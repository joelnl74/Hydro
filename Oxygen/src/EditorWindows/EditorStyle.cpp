#include "EditorStyle.h"

namespace Hydro
{
	static ImVec4 Rgb(int r, int g, int b, float a = 1.0f)
	{
		return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
	}

	// The Hydro theme: ImGui's dark preset with neutral greys and one blue accent.
	static void ApplyHydroColors(std::array<ImVec4, ImGuiCol_COUNT>& colors)
	{
		const ImVec4 background = Rgb(30, 31, 34);
		const ImVec4 panel = Rgb(37, 38, 42);
		const ImVec4 frame = Rgb(48, 50, 55);
		const ImVec4 frameHovered = Rgb(60, 63, 69);
		const ImVec4 frameActive = Rgb(70, 74, 82);
		const ImVec4 accent = Rgb(66, 135, 245);
		const ImVec4 accentHovered = Rgb(92, 154, 250);
		const ImVec4 accentActive = Rgb(48, 112, 220);

		colors[ImGuiCol_Text] = Rgb(222, 224, 228);
		colors[ImGuiCol_TextDisabled] = Rgb(128, 131, 138);
		colors[ImGuiCol_WindowBg] = background;
		colors[ImGuiCol_ChildBg] = Rgb(0, 0, 0, 0.0f);
		colors[ImGuiCol_PopupBg] = Rgb(34, 35, 39, 0.98f);
		colors[ImGuiCol_Border] = Rgb(55, 57, 62);
		colors[ImGuiCol_BorderShadow] = Rgb(0, 0, 0, 0.0f);
		colors[ImGuiCol_FrameBg] = frame;
		colors[ImGuiCol_FrameBgHovered] = frameHovered;
		colors[ImGuiCol_FrameBgActive] = frameActive;
		colors[ImGuiCol_TitleBg] = Rgb(24, 25, 27);
		colors[ImGuiCol_TitleBgActive] = Rgb(28, 29, 32);
		colors[ImGuiCol_TitleBgCollapsed] = Rgb(24, 25, 27);
		colors[ImGuiCol_MenuBarBg] = Rgb(26, 27, 30);
		colors[ImGuiCol_ScrollbarBg] = Rgb(26, 27, 30);
		colors[ImGuiCol_ScrollbarGrab] = Rgb(66, 69, 76);
		colors[ImGuiCol_ScrollbarGrabHovered] = Rgb(84, 88, 96);
		colors[ImGuiCol_ScrollbarGrabActive] = Rgb(100, 104, 114);
		colors[ImGuiCol_CheckMark] = accent;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentActive;
		colors[ImGuiCol_Button] = frame;
		colors[ImGuiCol_ButtonHovered] = frameHovered;
		colors[ImGuiCol_ButtonActive] = frameActive;
		colors[ImGuiCol_Header] = Rgb(52, 55, 61);
		colors[ImGuiCol_HeaderHovered] = Rgb(62, 66, 74);
		colors[ImGuiCol_HeaderActive] = Rgb(72, 77, 86);
		colors[ImGuiCol_Separator] = Rgb(55, 57, 62);
		colors[ImGuiCol_SeparatorHovered] = accentHovered;
		colors[ImGuiCol_SeparatorActive] = accent;
		colors[ImGuiCol_ResizeGrip] = Rgb(66, 135, 245, 0.20f);
		colors[ImGuiCol_ResizeGripHovered] = Rgb(66, 135, 245, 0.60f);
		colors[ImGuiCol_ResizeGripActive] = Rgb(66, 135, 245, 0.90f);
		colors[ImGuiCol_Tab] = panel;
		colors[ImGuiCol_TabHovered] = frameHovered;
		colors[ImGuiCol_TabActive] = Rgb(50, 53, 59);
		colors[ImGuiCol_TabUnfocused] = Rgb(30, 31, 34);
		colors[ImGuiCol_TabUnfocusedActive] = Rgb(42, 44, 49);
		colors[ImGuiCol_DockingPreview] = Rgb(66, 135, 245, 0.70f);
		colors[ImGuiCol_DockingEmptyBg] = Rgb(20, 21, 23);
		colors[ImGuiCol_PlotLines] = Rgb(150, 155, 165);
		colors[ImGuiCol_PlotLinesHovered] = accentHovered;
		colors[ImGuiCol_PlotHistogram] = accent;
		colors[ImGuiCol_PlotHistogramHovered] = accentHovered;
		colors[ImGuiCol_TableHeaderBg] = Rgb(40, 42, 46);
		colors[ImGuiCol_TableBorderStrong] = Rgb(55, 57, 62);
		colors[ImGuiCol_TableBorderLight] = Rgb(45, 47, 52);
		colors[ImGuiCol_TableRowBg] = Rgb(0, 0, 0, 0.0f);
		colors[ImGuiCol_TableRowBgAlt] = Rgb(255, 255, 255, 0.03f);
		colors[ImGuiCol_TextSelectedBg] = Rgb(66, 135, 245, 0.35f);
		colors[ImGuiCol_DragDropTarget] = accentHovered;
		colors[ImGuiCol_NavHighlight] = accent;
		colors[ImGuiCol_NavWindowingHighlight] = Rgb(255, 255, 255, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = Rgb(0, 0, 0, 0.40f);
		colors[ImGuiCol_ModalWindowDimBg] = Rgb(0, 0, 0, 0.50f);
	}

	EditorStyle EditorStyle::FromTheme(EditorTheme theme)
	{
		// Start from ImGui's preset (it fills every colour), then the editor's sizes.
		ImGuiStyle imguiStyle;
		switch (theme)
		{
		case EditorTheme::Light:   ImGui::StyleColorsLight(&imguiStyle); break;
		case EditorTheme::Classic: ImGui::StyleColorsClassic(&imguiStyle); break;
		case EditorTheme::Dark:
		case EditorTheme::Hydro:   ImGui::StyleColorsDark(&imguiStyle); break;
		}

		EditorStyle style;
		style.Theme = theme;
		for (int i = 0; i < ImGuiCol_COUNT; i++)
			style.Colors[i] = imguiStyle.Colors[i];

		if (theme == EditorTheme::Hydro)
		{
			ApplyHydroColors(style.Colors);
		}
		else
		{
			// ImGui's presets keep ImGui's own sizes.
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
			style.ItemSpacing = imguiStyle.ItemSpacing;
			style.ItemInnerSpacing = imguiStyle.ItemInnerSpacing;
			style.IndentSpacing = imguiStyle.IndentSpacing;
			style.ScrollbarSize = imguiStyle.ScrollbarSize;
			style.GrabMinSize = imguiStyle.GrabMinSize;
		}

		return style;
	}

	const char* EditorStyle::GetThemeName(EditorTheme theme)
	{
		switch (theme)
		{
		case EditorTheme::Hydro:   return "Hydro";
		case EditorTheme::Dark:    return "Dark";
		case EditorTheme::Light:   return "Light";
		case EditorTheme::Classic: return "Classic";
		}
		return "Hydro";
	}

	bool EditorStyle::TryParseTheme(const std::string& name, EditorTheme& out)
	{
		const EditorTheme themes[] = { EditorTheme::Hydro, EditorTheme::Dark, EditorTheme::Light, EditorTheme::Classic };
		for (EditorTheme theme : themes)
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
