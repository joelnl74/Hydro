#pragma once
#include <functional>

namespace Hydro
{
	class Scene;

	// The "Render Settings" window: edits the live renderer settings, the project's render features and the open
	// scene's override and look. Changes apply at once; File > Save Project or Save writes them to the .hyproj.
	class RenderSettingsWindow
	{
	public:
		// `editorScene` is edited and saved; `activeScene` (the running copy while playing) gets the same edits.
		// Either may be null.
		void OnImguiRender(Scene* editorScene, Scene* activeScene);

		bool& IsOpen() { return m_Open; }

		// Called by the Save button: the editor copies the live settings into the project and saves it.
		std::function<void()> OnSaveProject;

	private:
		// The scene's render profile, where each section comes from, and its fields.
		void DrawLook(Scene& editorScene, Scene* activeScene);

		bool m_Open = true;
	};
}
