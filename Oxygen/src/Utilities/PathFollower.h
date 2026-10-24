#pragma once
#include <filesystem>
#include <system_error>

namespace Hydro
{
	// Keeps a remembered path pointing at the same file or folder after it was renamed or moved.
	class PathFollower
	{
	public:
		// If `path` is `from` or inside it, points it at the same place under `to` and returns true.
		static bool Follow(std::filesystem::path& path, const std::filesystem::path& from, const std::filesystem::path& to)
		{
			if (path.empty())
			{
				return false;
			}

			std::error_code ec;
			const std::filesystem::path absolutePath = std::filesystem::absolute(path, ec).lexically_normal();
			const std::filesystem::path absoluteFrom = std::filesystem::absolute(from, ec).lexically_normal();
			const std::filesystem::path relative = absolutePath.lexically_relative(absoluteFrom);
			if (relative.empty() || *relative.begin() == "..")
			{
				return false;
			}

			const std::filesystem::path absoluteTo = std::filesystem::absolute(to, ec).lexically_normal();
			path = relative == "." ? absoluteTo : (absoluteTo / relative).lexically_normal();
			return true;
		}
	};
}
