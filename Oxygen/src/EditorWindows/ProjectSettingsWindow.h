#pragma once
#include <filesystem>
#include <functional>

namespace Hydro
{
	// The "Project Settings" window: the active project's name, start scene, scene template and file locations.
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
