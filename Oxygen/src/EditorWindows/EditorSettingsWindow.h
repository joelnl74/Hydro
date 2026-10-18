#pragma once
#include "../EditorSettings.h"

#include <imgui/imgui.h>

#include <functional>

namespace Hydro
{
	// "Editor Settings" window: how the editor looks (theme, colours, sizes, font scale). Changes
	// apply immediately; Save writes EditorSettings.yaml through OnSave, Revert reloads it through
	// OnRevert. The window doesn't touch files itself.
	class EditorSettingsWindow
	{
	public:
		void OnImguiRender(EditorSettings& settings);

		bool& IsOpen() { return m_Open; }

		std::function<void()> OnSave;
		std::function<void()> OnRevert;

	private:
		bool DrawThemeSection(EditorStyle& style);
		bool DrawSizesSection(EditorStyle& style);
		bool DrawColorsSection(EditorStyle& style);

	private:
		bool m_Open = false;
		bool m_Unsaved = false;
		ImGuiTextFilter m_ColorFilter;
	};
}
