#pragma once
#include "Hydro/Asset/Asset.h"

#include <filesystem>

namespace Hydro
{
	// What the AssetRegistry knows about one asset: which file it is and what kind.
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type = AssetType::None;
		// Relative to the project's asset folder, with forward slashes ("Scenes/Level1.hyscene").
		std::filesystem::path Path;
		// The file was registered earlier but is gone (deleted, or moved without its handle). Its entry
		// is kept, so a reference to it is reported as missing instead of quietly becoming "none".
		bool IsMissing = false;
	};
}
