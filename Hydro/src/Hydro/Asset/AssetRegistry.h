#pragma once
#include "Hydro/Asset/AssetMetadata.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Hydro
{
	// The project's assets: handle -> AssetMetadata, plus path -> handle. Not saved: the AssetManager rebuilds it from
	// the files whenever it scans the asset folder.
	class AssetRegistry
	{
	public:
		// Replaces an entry with the same handle (and whatever was at its path).
		void Add(const AssetMetadata& metadata);
		void Remove(AssetHandle handle);
		void Clear();

		bool Contains(AssetHandle handle) const;
		const AssetMetadata* Get(AssetHandle handle) const;
		// `relativePath` is relative to the asset folder; 0 when not registered.
		AssetHandle Find(const std::filesystem::path& relativePath) const;

		size_t GetCount() const { return m_Assets.size(); }
		const std::unordered_map<AssetHandle, AssetMetadata>& GetAll() const { return m_Assets; }

		// Normalised, forward slashes: how paths are compared.
		static std::string MakePathKey(const std::filesystem::path& relativePath);

	private:
		std::unordered_map<AssetHandle, AssetMetadata> m_Assets;
		std::unordered_map<std::string, AssetHandle> m_HandlesByPath;
	};
}
