#pragma once
#include "Hydro/Asset/Asset.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes where a file keeps its asset handle:
	//
	// - Files Hydro writes (scenes, prefabs) store it in the file itself, as its first line:
	//     Handle: 1234567890
	//     Scene: Level1
	//     ...
	// - Other files (images) get a sidecar next to them, "<file name>.hymeta" (YAML):
	//     Handle: 1234567890
	//     Type: Texture2D
	//   It moves with its file: move or rename both in Explorer and the handle stays. Commit sidecars.
	//   Other keys in a sidecar (texture import settings) are kept when the handle is written.
	class AssetMetaSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymeta";
		// The key of the handle line in scenes, prefabs and sidecars.
		static constexpr const char* HandleKey = "Handle";

		// True for the types that keep their handle inside the file (scenes, prefabs).
		static bool StoresHandleInFile(AssetType type);
		// "<file>.hymeta" for `file`.
		static std::filesystem::path GetSidecarPath(const std::filesystem::path& file);
		static bool IsSidecar(const std::filesystem::path& path);

		// The handle stored for `file` (in the file or its sidecar), or 0 when it has none or it can't be
		// read. `type` is the file's AssetType (GetAssetTypeFromExtension).
		static AssetHandle ReadHandle(const std::filesystem::path& file, AssetType type);
		// Stores `handle` for an existing `file`: writes its sidecar, or puts the handle line at the top of a
		// scene or prefab (replacing the one that is there). Returns false (logged) on failure.
		static bool WriteHandle(const std::filesystem::path& file, AssetType type, AssetHandle handle);
	};
}
