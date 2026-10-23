#include "hypch.h"
#include "AssetRegistry.h"

namespace Hydro
{
	void AssetRegistry::Add(const AssetMetadata& metadata)
	{
		Remove(metadata.Handle);

		const std::string key = MakePathKey(metadata.Path);
		if (auto existing = m_HandlesByPath.find(key); existing != m_HandlesByPath.end())
		{
			m_Assets.erase(existing->second);
		}

		AssetMetadata& stored = m_Assets[metadata.Handle];
		stored = metadata;
		stored.Path = std::filesystem::u8path(key);
		m_HandlesByPath[key] = metadata.Handle;
	}

	void AssetRegistry::Remove(AssetHandle handle)
	{
		auto found = m_Assets.find(handle);
		if (found == m_Assets.end())
		{
			return;
		}

		auto byPath = m_HandlesByPath.find(MakePathKey(found->second.Path));
		if (byPath != m_HandlesByPath.end() && (uint64_t)byPath->second == (uint64_t)handle)
		{
			m_HandlesByPath.erase(byPath);
		}

		m_Assets.erase(found);
	}

	void AssetRegistry::Clear()
	{
		m_Assets.clear();
		m_HandlesByPath.clear();
	}

	bool AssetRegistry::Contains(AssetHandle handle) const
	{
		return m_Assets.find(handle) != m_Assets.end();
	}

	const AssetMetadata* AssetRegistry::Get(AssetHandle handle) const
	{
		auto found = m_Assets.find(handle);
		if (found == m_Assets.end())
		{
			return nullptr;
		}

		return &found->second;
	}

	AssetHandle AssetRegistry::Find(const std::filesystem::path& relativePath) const
	{
		auto found = m_HandlesByPath.find(MakePathKey(relativePath));
		if (found == m_HandlesByPath.end())
		{
			return 0;
		}

		return found->second;
	}

	std::string AssetRegistry::MakePathKey(const std::filesystem::path& relativePath)
	{
		return relativePath.lexically_normal().generic_u8string();
	}
}
