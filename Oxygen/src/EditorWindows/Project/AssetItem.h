#pragma once
#include "Hydro/Asset/Asset.h"

#include <filesystem>
#include <string>

namespace Hydro
{
	// What the Project window shows an item as (an AssetType, plus folders).
	enum class AssetItemType { Folder, Scene, Prefab, Texture, Animation, Material, Model, RenderProfile, Font, Terrain,
		Definition, DataTable };

	// One tile of the Project window: a folder on disk or a registered asset.
	struct AssetItem
	{
		std::filesystem::path Path; // absolute
		std::string DisplayName;
		AssetItemType Type = AssetItemType::Folder;
		AssetHandle Handle = 0;     // 0 for folders
	};
}
