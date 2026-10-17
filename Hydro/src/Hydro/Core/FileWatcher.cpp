#include "hypch.h"
#include "FileWatcher.h"

namespace Hydro
{
	FileWatcher::FileWatcher(const std::vector<std::filesystem::path>& files)
		: m_Paths(files)
	{
		m_States.reserve(m_Paths.size());
		for (const auto& path : m_Paths)
			m_States.push_back(ReadState(path));
	}

	bool FileWatcher::Poll()
	{
		bool changed = false;
		for (size_t i = 0; i < m_Paths.size(); i++)
		{
			const FileState state = ReadState(m_Paths[i]);
			if ((state == m_States[i]) == false)
			{
				m_States[i] = state;
				changed = true;
			}
		}

		return changed;
	}

	FileWatcher::FileState FileWatcher::ReadState(const std::filesystem::path& path)
	{
		// error_code overloads: a file that is missing or locked mid-save must not throw.
		FileState state;
		std::error_code error;
		state.LastWrite = std::filesystem::last_write_time(path, error);
		state.Exists = static_cast<bool>(error) == false;
		if (state.Exists == false)
			state.LastWrite = {};

		return state;
	}
}
