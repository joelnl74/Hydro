#pragma once
#include <functional>

namespace Hydro
{
	class Scene;

	// "Render Settings" window: edits Renderer::GetSettings() live (clear colour, vsync, wireframe,
	// shader hot reload, the project's render features), the open scene's render-feature override and look
	// (render profile, environment, post-processing), and shows what the GPU supports. Changes apply immediately; they are
	// written to the project (.hyproj) by File > Save Project or the Save button here.
	class RenderSettingsWindow
	{
	public:
		// `editorScene`: the scene that is edited and saved (its render settings are shown); `activeScene`:
		// the one being drawn (the running copy while playing), which gets the same edits. Either may be null.
		void OnImguiRender(Scene* editorScene, Scene* activeScene);

		bool& IsOpen() { return m_Open; }

		// Called by the Save button. The editor copies the live settings into the project and saves it.
		std::function<void()> OnSaveProject;

	private:
		// Look (scene): the render profile, where each section comes from, and its fields (post-processing plan P4).
		void DrawLook(Scene& editorScene, Scene* activeScene);

		bool m_Open = true;
	};
}
