#pragma once
#include <functional>

namespace Hydro
{
	// "Render Settings" window: edits Renderer::GetSettings() live (clear colour, vsync, wireframe,
	// shader hot reload) and shows what the GPU supports. Changes apply immediately; they are
	// written to the project (.hyproj) by File > Save Project or the Save button here.
	class RenderSettingsWindow
	{
	public:
		void OnImguiRender();

		bool& IsOpen() { return m_Open; }

		// Called by the Save button. The editor copies the live settings into the project and saves it.
		std::function<void()> OnSaveProject;

	private:
		bool m_Open = true;
	};
}
