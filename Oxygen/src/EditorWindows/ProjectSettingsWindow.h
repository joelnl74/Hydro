#pragma once
#include <filesystem>
#include <functional>

namespace Hydro
{
	// "Project Settings" window: the active project's general settings (name, start scene) and
	// where its files are. Render settings have their own window (RenderSettingsWindow).
	class ProjectSettingsWindow
	{
	public:
		// currentScenePath: the scene open in the editor (empty if unsaved), for "Use current scene".
		void OnImguiRender(const std::filesystem::path& currentScenePath);

		bool& IsOpen() { return m_Open; }

		std::function<void()> OnSaveProject;

	private:
		bool m_Open = false;
	};
}
