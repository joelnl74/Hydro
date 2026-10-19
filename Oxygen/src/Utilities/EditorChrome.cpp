#include "EditorChrome.h"
#include "../EditorSettings.h"

#include <Hydro/ImGUI/ImGuiLayer.h>

#include <imgui/imgui_internal.h>

#include <cmath>

namespace Hydro
{
	int EditorChrome::s_PushedTintColors = 0;

	static ImVec4 MixColor(const ImVec4& a, const ImVec4& b, float t)
	{
		return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w);
	}

	ImFont* EditorChrome::GetBoldFont()
	{
		return ImGuiLayer::GetFont("Bold");
	}

	// A water drop: a circle with a point on top. `centre` is the circle's centre.
	static void DrawDroplet(ImDrawList* drawList, const ImVec2& centre, float radius, ImU32 color)
	{
		const float pi = 3.14159265f;
		const float tipDistance = radius * 2.1f;
		// Angle between "towards the tip" and the points where the sides touch the circle.
		const float side = std::acos(radius / tipDistance);

		drawList->PathLineTo(ImVec2(centre.x, centre.y - tipDistance));
		drawList->PathArcTo(centre, radius, -pi * 0.5f + side, pi * 1.5f - side, 24);
		drawList->PathFillConvex(color);

		// A small highlight, like light on water.
		drawList->AddCircleFilled(ImVec2(centre.x - radius * 0.35f, centre.y - radius * 0.2f), radius * 0.28f, IM_COL32(255, 255, 255, 90), 12);
	}

	void EditorChrome::DrawBrand(const ImVec4& accent)
	{
		const float height = ImGui::GetFrameHeight();
		const float radius = height * 0.22f;
		const ImVec2 size(radius * 2.0f + 8.0f, height);

		const ImVec2 position = ImGui::GetCursorScreenPos();
		ImGui::Dummy(size);
		const ImVec2 centre(position.x + size.x * 0.5f, position.y + height * 0.62f);
		DrawDroplet(ImGui::GetWindowDrawList(), centre, radius, ImGui::GetColorU32(accent));

		ImFont* bold = GetBoldFont();
		if (bold != nullptr)
			ImGui::PushFont(bold);
		ImGui::TextUnformatted("Oxygen");
		if (bold != nullptr)
			ImGui::PopFont();

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Oxygen editor - Hydro engine");
	}

	void EditorChrome::DrawMenuBarWaterline(const ImVec4& color)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImRect bar = window->MenuBarRect();

		const float thickness = 2.0f;
		const ImVec2 min(bar.Min.x, bar.Max.y - thickness);
		const ImVec2 max(bar.Max.x, bar.Max.y);

		const ImU32 strong = ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 1.0f));
		const ImU32 faint = ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.12f));

		// The menu bar clips to itself; draw over the whole bar.
		window->DrawList->PushClipRect(bar.Min, bar.Max, false);
		window->DrawList->AddRectFilledMultiColor(min, max, strong, faint, faint, strong);
		window->DrawList->PopClipRect();
	}

	static void DrawIcon(ImDrawList* drawList, ToolbarIcon icon, const ImVec2& min, const ImVec2& max, ImU32 color)
	{
		const ImVec2 centre((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
		const float s = (max.y - min.y) * 0.22f; // half the icon's size

		switch (icon)
		{
		case ToolbarIcon::Play:
			drawList->AddTriangleFilled(ImVec2(centre.x - s * 0.8f, centre.y - s), ImVec2(centre.x - s * 0.8f, centre.y + s),
				ImVec2(centre.x + s, centre.y), color);
			break;
		case ToolbarIcon::Stop:
			drawList->AddRectFilled(ImVec2(centre.x - s * 0.85f, centre.y - s * 0.85f), ImVec2(centre.x + s * 0.85f, centre.y + s * 0.85f), color, 1.5f);
			break;
		case ToolbarIcon::Pause:
			drawList->AddRectFilled(ImVec2(centre.x - s * 0.8f, centre.y - s), ImVec2(centre.x - s * 0.25f, centre.y + s), color, 1.0f);
			drawList->AddRectFilled(ImVec2(centre.x + s * 0.25f, centre.y - s), ImVec2(centre.x + s * 0.8f, centre.y + s), color, 1.0f);
			break;
		case ToolbarIcon::Step:
			drawList->AddTriangleFilled(ImVec2(centre.x - s, centre.y - s), ImVec2(centre.x - s, centre.y + s),
				ImVec2(centre.x + s * 0.45f, centre.y), color);
			drawList->AddRectFilled(ImVec2(centre.x + s * 0.5f, centre.y - s), ImVec2(centre.x + s, centre.y + s), color, 1.0f);
			break;
		}
	}

	bool EditorChrome::IconButton(const char* id, ToolbarIcon icon, float size, bool highlighted, const ImVec4& highlightColor, const char* tooltip)
	{
		if (highlighted)
		{
			const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, highlightColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MixColor(highlightColor, white, 0.15f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, MixColor(highlightColor, white, 0.30f));
		}

		const bool pressed = ImGui::Button(id, ImVec2(size, ImGui::GetFrameHeight()));

		if (highlighted)
			ImGui::PopStyleColor(3);

		// Dark icon on a bright highlight, the text colour otherwise (dimmed when disabled).
		const float luminance = 0.2126f * highlightColor.x + 0.7152f * highlightColor.y + 0.0722f * highlightColor.z;
		ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
		if (highlighted)
			color = luminance > 0.55f ? IM_COL32(12, 16, 22, 255) : IM_COL32(255, 255, 255, 255);

		DrawIcon(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color);

		if (tooltip != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", tooltip);

		return pressed;
	}

	void EditorChrome::OutlineLastItem(const ImVec4& color, float thickness)
	{
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const float inset = thickness * 0.5f;
		ImGui::GetWindowDrawList()->AddRect(ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, max.y - inset),
			ImGui::GetColorU32(color), 0.0f, 0, thickness);
	}

	void EditorChrome::PushPlayModeTint(const PlayModeSettings& settings, bool playing)
	{
		s_PushedTintColors = 0;
		if (playing == false || settings.Tint == false || settings.TintStrength <= 0.0f)
			return;

		const ImGuiCol tinted[] =
		{
			ImGuiCol_WindowBg, ImGuiCol_MenuBarBg, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed,
			ImGuiCol_Tab, ImGuiCol_TabActive, ImGuiCol_TabUnfocused, ImGuiCol_TabUnfocusedActive, ImGuiCol_DockingEmptyBg
		};

		const ImGuiStyle& style = ImGui::GetStyle();
		for (ImGuiCol colorIndex : tinted)
		{
			ImGui::PushStyleColor(colorIndex, MixColor(style.Colors[colorIndex], settings.TintColor, settings.TintStrength));
			s_PushedTintColors++;
		}
	}

	void EditorChrome::PopPlayModeTint()
	{
		if (s_PushedTintColors > 0)
			ImGui::PopStyleColor(s_PushedTintColors);
		s_PushedTintColors = 0;
	}
}
