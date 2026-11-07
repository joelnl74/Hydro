#pragma once
#include "../RecentProjects.h"

#include <filesystem>
#include <functional>

namespace Hydro
{
	// Opening another game's project: this Oxygen can't read that game's scripts, components and data, and saving would
	// drop them, so it offers to open it read-only, to start that game's editor, or to cancel. No editable way in.
	class GameMismatchDialog
	{
	public:
		std::function<void(const std::filesystem::path&)> OnOpenReadOnly;

		void Open(const std::filesystem::path& project, const GameInfo& game);
		// Drawn every frame, before the hub and the dockspace; the recent list knows which Oxygen opened the project.
		void OnImguiRender(const RecentProjects& recent);

	private:
		// The Oxygen that last opened this project, or another project of its game; empty when none is known.
		std::filesystem::path FindEditor(const RecentProjects& recent) const;
		void Launch(const std::filesystem::path& editor) const;

	private:
		std::filesystem::path m_Project;
		GameInfo m_Game;
		bool m_OpenRequested = false;
	};
}
