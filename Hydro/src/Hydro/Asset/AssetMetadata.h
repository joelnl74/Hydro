#pragma once
#include "Hydro/Asset/Asset.h"

#include <filesystem>

namespace Hydro
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type = AssetType::None;
		// Relative to the project's asset folder, with forward slashes ("Scenes/Level1.hyscene").
		std::filesystem::path Path;
		// Registered earlier but gone; kept so a reference reports a missing asset instead of quietly showing "none".
		bool IsMissing = false;
	};
}
