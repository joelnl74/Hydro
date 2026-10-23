#include "hypch.h"
#include "Asset.h"

#include <cctype>

namespace Hydro
{
	const char* AssetTypeToString(AssetType type)
	{
		switch (type)
		{
			case AssetType::Texture2D: return "Texture2D";
			case AssetType::Scene: return "Scene";
			case AssetType::Prefab: return "Prefab";
			case AssetType::None: return "None";
		}

		return "None";
	}

	AssetType AssetTypeFromString(const std::string& name)
	{
		if (name == "Texture2D")
		{
			return AssetType::Texture2D;
		}
		if (name == "Scene")
		{
			return AssetType::Scene;
		}
		if (name == "Prefab")
		{
			return AssetType::Prefab;
		}

		return AssetType::None;
	}

	AssetType GetAssetTypeFromExtension(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		for (char& c : extension)
		{
			c = (char)std::tolower((unsigned char)c);
		}

		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp")
		{
			return AssetType::Texture2D;
		}
		// The same strings as SceneSerializer::FileExtension and PrefabSerializer::FileExtension; the
		// asset layer sits below Scene, so it doesn't include those headers.
		if (extension == ".hyscene")
		{
			return AssetType::Scene;
		}
		if (extension == ".hyprefab")
		{
			return AssetType::Prefab;
		}

		return AssetType::None;
	}
}
