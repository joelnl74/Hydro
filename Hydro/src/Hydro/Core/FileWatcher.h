#pragma once

#include <filesystem>
#include <vector>

namespace Hydro
{
	// Watches a fixed set of files by comparing their last-write times when Poll() is called.
	// Meant for a handful of files polled a few times per second (shader hot reload), not for
	// watching whole directories.
	class FileWatcher
	{
	public:
		FileWatcher() = default;
		explicit FileWatcher(const std::vector<std::filesystem::path>& files);

		// True when any watched file was modified, created or deleted since the previous call
		// (or since construction). Never throws.
		bool Poll();

		const std::vector<std::filesystem::path>& GetFiles() const { return m_Paths; }

	private:
		struct FileState
		{
			bool Exists = false;
			std::filesystem::file_time_type LastWrite{};

			bool operator==(const FileState& other) const { return Exists == other.Exists && LastWrite == other.LastWrite; }
		};

		static FileState ReadState(const std::filesystem::path& path);

	private:
		std::vector<std::filesystem::path> m_Paths;
		std::vector<FileState> m_States;
	};
}
