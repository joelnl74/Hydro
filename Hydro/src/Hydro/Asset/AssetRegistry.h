#pragma once
#include "Hydro/Asset/AssetMetadata.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Hydro
{
	// The project's assets: handle -> AssetMetadata, plus path -> handle. Plain data, not a subsystem
	// and not saved: the AssetManager rebuilds it from the files (their "Handle:" lines and .hymeta
	// sidecars) whenever it scans the asset folder, so the files are the source of truth.
	class AssetRegistry
	{
	public:
		// Adds `metadata`, replacing an entry with the same handle (and whatever was at its path).
		void Add(const AssetMetadata& metadata);
		void Remove(AssetHandle handle);
		void Clear();

		bool Contains(AssetHandle handle) const;
		// nullptr when the handle isn't registered.
		const AssetMetadata* Get(AssetHandle handle) const;
		// The handle of the asset at `relativePath` (relative to the asset folder), or 0.
		AssetHandle Find(const std::filesystem::path& relativePath) const;

		size_t GetCount() const { return m_Assets.size(); }
		const std::unordered_map<AssetHandle, AssetMetadata>& GetAll() const { return m_Assets; }

		// The key paths are compared by: normalised, forward slashes.
		static std::string MakePathKey(const std::filesystem::path& relativePath);

	private:
		std::unordered_map<AssetHandle, AssetMetadata> m_Assets;
		std::unordered_map<std::string, AssetHandle> m_HandlesByPath;
	};
}
