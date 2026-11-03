#pragma once
#include "../EditorSettings.h"

#include <imgui/imgui.h>

#include <cstdint>
#include <functional>

namespace Hydro
{
	// The "Editor Settings" window: how the editor looks and behaves. Changes apply at once; Save and Revert go
	// through OnSave and OnRevert, so the window never touches files itself.
	class EditorSettingsWindow
	{
	public:
		void OnImguiRender(EditorSettings& settings);

		bool& IsOpen() { return m_Open; }

		std::function<void()> OnSave;
		std::function<void()> OnRevert;

	private:
		enum class Page : uint8_t
		{
			Appearance,
			PlayMode,
			Layout,
			Colors,
			EditorCamera,
			Console,
			Startup
		};

		bool DrawPageList();
		bool DrawPage(EditorSettings& settings);
		void DrawFooter(EditorSettings& settings);

		bool DrawAppearancePage(EditorSettings& settings);
		bool DrawPlayModePage(EditorSettings& settings);
		bool DrawLayoutPage(EditorStyle& style);
		bool DrawColorsPage(EditorStyle& style);
		bool DrawEditorCameraPage(EditorSettings& settings);
		bool DrawConsolePage(ConsoleSettings& console);
		bool DrawStartupPage(EditorSettings& settings);

	private:
		bool m_Open = false;
		bool m_Unsaved = false;
		Page m_Page = Page::Appearance;
		ImGuiTextFilter m_ColorFilter;
	};
}
