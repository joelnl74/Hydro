#pragma once
#include "Hydro/Asset/Asset.h"

#include <filesystem>

namespace Hydro
{
	// Where a file keeps its asset handle: files Hydro writes have a "Handle:" first line, other files (images) a
	// "<file>.hymeta" sidecar that moves with them (other keys in it are kept when the handle is written).
	class AssetMetaSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymeta";
		// The key of the handle line in scenes, prefabs and sidecars.
		static constexpr const char* HandleKey = "Handle";

		// Scenes, prefabs, .hyanim, .hymat, .hyrender, .hyterrain, .hydef and .hytable.
		static bool StoresHandleInFile(AssetType type);
		static std::filesystem::path GetSidecarPath(const std::filesystem::path& file);
		static bool IsSidecar(const std::filesystem::path& path);

		// From the file or its sidecar; 0 when it has none or it can't be read.
		static AssetHandle ReadHandle(const std::filesystem::path& file, AssetType type);
		// Writes the sidecar or replaces the handle line at the top of the file. False (logged) on failure.
		static bool WriteHandle(const std::filesystem::path& file, AssetType type, AssetHandle handle);
	};
}
