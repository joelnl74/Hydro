// A stand-in for the AssetManager in the headless data tests (scripts/test_data.sh): only what Hydro/Data uses
// (GetMetadata, GetAbsolutePath, GetAsset of definitions and tables), over files the test registers. Like the real
// one it loads a file again when its modification time changed, and holders of the old object keep it.
// The real AssetManager pulls in the renderer, so it can't be built here.

#include "hypch.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Data/DataTableAsset.h"
#include "Hydro/Data/DefinitionAsset.h"

#include <unordered_map>

namespace Hydro
{
	struct TestAssetEntry
	{
		AssetMetadata Metadata;
		std::filesystem::path Absolute;
		Ref<Asset> Loaded;
		std::filesystem::file_time_type Version{};
		int LoadCount = 0;
	};

	static std::unordered_map<uint64_t, TestAssetEntry>& GetTestAssets()
	{
		static std::unordered_map<uint64_t, TestAssetEntry> assets;
		return assets;
	}

	// The test's way in: register a file under a handle (or mark a registered one missing).
	void TestRegisterAsset(AssetHandle handle, const std::filesystem::path& absolute, const std::string& relative)
	{
		TestAssetEntry entry;
		entry.Metadata.Handle = handle;
		entry.Metadata.Type = GetAssetTypeFromExtension(absolute);
		entry.Metadata.Path = relative;
		entry.Absolute = absolute;
		GetTestAssets()[(uint64_t)handle] = entry;
	}

	void TestMarkMissing(AssetHandle handle)
	{
		GetTestAssets()[(uint64_t)handle].Metadata.IsMissing = true;
	}

	int TestGetLoadCount(AssetHandle handle)
	{
		return GetTestAssets()[(uint64_t)handle].LoadCount;
	}

	const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle)
	{
		auto it = GetTestAssets().find((uint64_t)handle);
		return it == GetTestAssets().end() ? nullptr : &it->second.Metadata;
	}

	std::filesystem::path AssetManager::GetAbsolutePath(AssetHandle handle)
	{
		auto it = GetTestAssets().find((uint64_t)handle);
		return it == GetTestAssets().end() ? std::filesystem::path() : it->second.Absolute;
	}

	Ref<Asset> AssetManager::LoadAsset(AssetHandle handle, AssetType type, bool force)
	{
		auto it = GetTestAssets().find((uint64_t)handle);
		if (it == GetTestAssets().end() || it->second.Metadata.IsMissing || it->second.Metadata.Type != type)
		{
			return nullptr;
		}

		TestAssetEntry& entry = it->second;
		std::error_code ec;
		const std::filesystem::file_time_type version = std::filesystem::last_write_time(entry.Absolute, ec);
		if (force == false && entry.LoadCount > 0 && version == entry.Version)
		{
			return entry.Loaded;
		}

		Ref<Asset> loaded;
		if (type == AssetType::Definition)
		{
			loaded = DefinitionAsset::LoadUncached(entry.Absolute);
		}
		else if (type == AssetType::DataTable)
		{
			loaded = DataTableAsset::LoadUncached(entry.Absolute);
		}
		entry.LoadCount++;
		entry.Version = version;
		if (loaded != nullptr)
		{
			loaded->m_Handle = handle;
			entry.Loaded = loaded;
		}
		return entry.Loaded; // a broken file keeps the last good object, like the real manager
	}
}
