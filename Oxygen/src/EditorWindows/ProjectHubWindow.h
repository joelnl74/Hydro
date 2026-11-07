#pragma once
#include "../RecentProjects.h"

#include <array>
#include <filesystem>
#include <functional>
#include <string>

namespace Hydro
{
	// The project hub: recent projects grouped by whether this Oxygen's game can edit them, New Project and Open. Fills
	// the window when no project is open (File > Project Hub over one); EditorLayer creates and opens through the callbacks.
	class ProjectHubWindow
	{
	public:
		std::function<void(const std::filesystem::path&)> OnOpenProject;
		// Name and the folder the project's own folder is made in.
		std::function<void(const std::string&, const std::filesystem::path&)> OnNewProject;
		std::function<void()> OnBrowseProject;
		// A recent entry was removed or relocated: the list is to be saved.
		std::function<void()> OnRecentChanged;

		// projectOpen: there is a project to go back to (the hub can be closed).
		void OnImguiRender(RecentProjects& recent, bool projectOpen);

		void Open(bool newProject = false);
		void Close() { m_Open = false; }
		bool IsOpen() const { return m_Open; }

	private:
		void DrawNewProject(const RecentProjects& recent);
		void DrawGroup(const RecentProjects& recent, ProjectCompatibility compatibility, const char* title);
		void DrawEntry(const RecentProject& entry);

	private:
		bool m_Open = false;
		bool m_ShowNewProject = false;
		std::array<char, 128> m_NewName{};
		std::array<char, 512> m_NewLocation{};
		// Acted on after the list is drawn: opening or removing changes the list being drawn.
		std::filesystem::path m_PendingOpen;
		std::filesystem::path m_PendingRemove;
	};
}
