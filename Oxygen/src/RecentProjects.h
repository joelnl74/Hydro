#pragma once
#include <Hydro/Project/Project.h>

#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// How a project relates to the game compiled into this Oxygen.
	enum class ProjectCompatibility : uint8_t
	{
		Compatible, // made with this game
		Generic,    // no game types yet: any build can open it
		OtherGame   // another game's project: this build can't edit it safely
	};

	// A project the editor opened, as remembered in the hub.
	struct RecentProject
	{
		std::filesystem::path Path; // the .hyproj, absolute
		std::string Name;
		GameInfo Game;
		std::string LastOpened;      // local time, "YYYY-MM-DD HH:MM"
		std::filesystem::path Editor; // the Oxygen that last opened it (to launch another game's editor)

		ProjectCompatibility GetCompatibility() const;
	};

	// The hub's recent projects, most recent first, per user (RecentProjects.yaml in EditorPaths::GetUserDirectory()).
	class RecentProjects
	{
	public:
		static constexpr size_t MaxCount = 10;

		// Missing or unreadable file: an empty list (logged when unreadable).
		void Load(const std::filesystem::path& path);
		bool Save(const std::filesystem::path& path) const;

		// Moves the project to the top (stamped with the current time), replacing an entry with the same path.
		void Add(RecentProject project);
		void Remove(const std::filesystem::path& path);

		const std::vector<RecentProject>& GetEntries() const { return m_Entries; }

		// The local time as "YYYY-MM-DD HH:MM".
		static std::string Now();

	private:
		std::vector<RecentProject> m_Entries;
	};
}
