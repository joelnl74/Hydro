#include "hypch.h"
#include "AssetManager.h"
#include "AssetMetaSerializer.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	struct AssetManagerData
	{
		std::filesystem::path AssetDirectory; // absolute, normalised; empty = no project
		AssetRegistry Registry;
	};

	static AssetManagerData* s_Data = nullptr;
	static const std::filesystem::path s_EmptyPath;
	static const AssetRegistry s_EmptyRegistry;

	// `file` relative to the asset folder, or empty when it is outside it (or no project is open).
	static std::filesystem::path ToRelativePath(const std::filesystem::path& file)
	{
		if (s_Data == nullptr || s_Data->AssetDirectory.empty())
		{
			return {};
		}

		std::error_code ec;
		std::filesystem::path absolute = file.is_absolute() ? file : s_Data->AssetDirectory / file;
		absolute = std::filesystem::absolute(absolute, ec).lexically_normal();

		std::filesystem::path relative = absolute.lexically_relative(s_Data->AssetDirectory);
		if (relative.empty() || *relative.begin() == "..")
		{
			return {};
		}

		return relative;
	}

	static std::filesystem::path ToAbsolutePath(const std::filesystem::path& file)
	{
		if (file.is_absolute() || s_Data == nullptr || s_Data->AssetDirectory.empty())
		{
			return file;
		}

		return s_Data->AssetDirectory / file;
	}

	void AssetManager::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "AssetManager::Init called twice");

		s_Data = new AssetManagerData();
	}

	void AssetManager::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}

	void AssetManager::OnProjectOpened(const std::filesystem::path& assetDirectory)
	{
		HY_CORE_ASSERT(s_Data != nullptr, "AssetManager::OnProjectOpened before AssetManager::Init");

		std::error_code ec;
		s_Data->AssetDirectory = std::filesystem::absolute(assetDirectory, ec).lexically_normal();
		s_Data->Registry.Clear();

		Rescan();
	}

	void AssetManager::OnProjectClosed()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		s_Data->AssetDirectory.clear();
		s_Data->Registry.Clear();
	}

	bool AssetManager::IsProjectOpen()
	{
		return s_Data != nullptr && s_Data->AssetDirectory.empty() == false;
	}

	const std::filesystem::path& AssetManager::GetAssetDirectory()
	{
		return s_Data != nullptr ? s_Data->AssetDirectory : s_EmptyPath;
	}

	AssetScanInfo AssetManager::Rescan()
	{
		AssetScanInfo info;
		if (IsProjectOpen() == false)
		{
			return info;
		}

		const std::filesystem::path& root = s_Data->AssetDirectory;

		struct FoundFile
		{
			std::filesystem::path Absolute;
			std::string Key; // relative path, forward slashes (AssetRegistry::MakePathKey)
			AssetType Type = AssetType::None;
			AssetHandle Handle = 0;
			bool IsCopy = false; // had another file's handle
		};

		std::vector<FoundFile> files;
		std::vector<std::filesystem::path> sidecars;

		std::error_code ec;
		const auto options = std::filesystem::directory_options::skip_permission_denied;
		for (std::filesystem::recursive_directory_iterator it(root, options, ec), end; ec.value() == 0 && it != end; it.increment(ec))
		{
			const std::filesystem::path& path = it->path();
			const std::string fileName = path.filename().u8string();
			if (fileName.empty() || fileName[0] == '.')
			{
				// Hidden files and folders (.git, .vs): not assets.
				if (it->is_directory(ec))
				{
					it.disable_recursion_pending();
				}
				continue;
			}

			if (it->is_regular_file(ec) == false)
			{
				continue;
			}

			if (AssetMetaSerializer::IsSidecar(path))
			{
				sidecars.push_back(path);
				continue;
			}

			const AssetType type = GetAssetTypeFromExtension(path);
			if (type == AssetType::None)
			{
				continue;
			}

			FoundFile file;
			file.Absolute = path;
			file.Key = AssetRegistry::MakePathKey(path.lexically_relative(root));
			file.Type = type;
			file.Handle = AssetMetaSerializer::ReadHandle(path, type);
			files.push_back(std::move(file));
		}

		if (ec)
		{
			HY_CORE_WARN("AssetManager: can't fully scan '{}': {}", root.string(), ec.message());
		}

		// Alphabetical, so which copy keeps a duplicated handle doesn't depend on the file system's order.
		std::sort(files.begin(), files.end(), [](const FoundFile& a, const FoundFile& b) { return a.Key < b.Key; });

		const AssetRegistry& previous = s_Data->Registry;

		// The file that keeps each handle: the one at the path registered before, otherwise the first.
		std::unordered_map<uint64_t, size_t> owners;
		for (size_t i = 0; i < files.size(); i++)
		{
			const uint64_t handle = (uint64_t)files[i].Handle;
			if (handle == 0)
			{
				continue;
			}

			auto owner = owners.find(handle);
			if (owner == owners.end())
			{
				owners[handle] = i;
				continue;
			}

			const AssetMetadata* registered = previous.Get(files[i].Handle);
			const bool wasHere = registered != nullptr && AssetRegistry::MakePathKey(registered->Path) == files[i].Key;
			const size_t loser = wasHere ? owner->second : i;
			if (wasHere)
			{
				owner->second = i;
			}

			HY_CORE_WARN("AssetManager: '{}' had the same asset handle as '{}' (a copy?); it gets a new one",
				files[loser].Key, files[owners[handle]].Key);
			files[loser].Handle = 0;
			files[loser].IsCopy = true;
			info.DuplicateHandles++;
		}

		AssetRegistry registry;
		for (FoundFile& file : files)
		{
			if ((uint64_t)file.Handle == 0)
			{
				file.Handle = AssetHandle();
				if (AssetMetaSerializer::WriteHandle(file.Absolute, file.Type, file.Handle) && file.IsCopy == false)
				{
					info.NewHandles++;
				}
				// On failure (logged) the file is still registered for this session; it gets another handle
				// next time.
			}

			AssetMetadata metadata;
			metadata.Handle = file.Handle;
			metadata.Type = file.Type;
			metadata.Path = std::filesystem::u8path(file.Key);
			registry.Add(metadata);
		}
		info.AssetCount = registry.GetCount();

		// Registered before, gone now: keep the handle, marked missing.
		for (const auto& [handle, metadata] : previous.GetAll())
		{
			if (registry.Contains(handle) == false)
			{
				AssetMetadata missing = metadata;
				missing.IsMissing = true;
				if ((uint64_t)registry.Find(missing.Path) == 0)
				{
					registry.Add(missing);
					info.MissingAssets++;
				}
			}
		}

		for (const std::filesystem::path& sidecar : sidecars)
		{
			std::filesystem::path file = sidecar;
			file.replace_extension();
			if (std::filesystem::exists(file, ec) == false)
			{
				HY_CORE_WARN("AssetManager: '{}' has no file next to it (moved without its .hymeta?)", sidecar.lexically_relative(root).generic_u8string());
				info.OrphanSidecars++;
			}
		}

		s_Data->Registry = std::move(registry);

		HY_CORE_INFO("Assets: {} in {} ({} new handles, {} duplicates, {} missing)", info.AssetCount, root.string(),
			info.NewHandles, info.DuplicateHandles, info.MissingAssets);
		return info;
	}

	AssetHandle AssetManager::ImportAsset(const std::filesystem::path& file)
	{
		const AssetType type = GetAssetTypeFromExtension(file);
		if (type == AssetType::None)
		{
			return 0;
		}

		const std::filesystem::path absolute = ToAbsolutePath(file);
		const std::filesystem::path relative = ToRelativePath(file);
		const std::string key = relative.empty() ? std::string() : AssetRegistry::MakePathKey(relative);

		if (key.empty() == false)
		{
			const AssetHandle registered = s_Data->Registry.Find(relative);
			const AssetMetadata* metadata = s_Data->Registry.Get(registered);
			if (metadata != nullptr && metadata->IsMissing == false)
			{
				return registered;
			}
		}

		std::error_code ec;
		const bool exists = std::filesystem::exists(absolute, ec);
		if (exists == false && AssetMetaSerializer::StoresHandleInFile(type) == false)
		{
			return 0;
		}

		AssetHandle handle = exists ? AssetMetaSerializer::ReadHandle(absolute, type) : AssetHandle(0);

		// Another file that still exists has this handle: this one is a copy.
		if ((uint64_t)handle != 0 && s_Data != nullptr)
		{
			if (const AssetMetadata* other = s_Data->Registry.Get(handle))
			{
				if (other->IsMissing == false && AssetRegistry::MakePathKey(other->Path) != key)
				{
					HY_CORE_WARN("AssetManager: '{}' has the same asset handle as '{}' (a copy?); it gets a new one",
						absolute.string(), other->Path.generic_u8string());
					handle = 0;
				}
			}
		}

		if ((uint64_t)handle == 0)
		{
			handle = AssetHandle();
			if (exists)
			{
				AssetMetaSerializer::WriteHandle(absolute, type, handle);
			}
		}

		if (key.empty() == false)
		{
			AssetMetadata metadata;
			metadata.Handle = handle;
			metadata.Type = type;
			metadata.Path = relative;
			s_Data->Registry.Add(metadata);
		}

		return handle;
	}

	void AssetManager::BindHandle(Asset& asset, const std::filesystem::path& file)
	{
		if (s_Data == nullptr)
		{
			return;
		}

		asset.m_Handle = ImportAsset(file);
	}

	AssetHandle AssetManager::FindHandle(const std::filesystem::path& file)
	{
		const std::filesystem::path relative = ToRelativePath(file);
		if (relative.empty())
		{
			return 0;
		}

		return s_Data->Registry.Find(relative);
	}

	const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle)
	{
		if (s_Data == nullptr)
		{
			return nullptr;
		}

		return s_Data->Registry.Get(handle);
	}

	bool AssetManager::IsAssetHandleValid(AssetHandle handle)
	{
		const AssetMetadata* metadata = GetMetadata(handle);
		return metadata != nullptr && metadata->IsMissing == false;
	}

	std::filesystem::path AssetManager::GetAbsolutePath(AssetHandle handle)
	{
		const AssetMetadata* metadata = GetMetadata(handle);
		if (metadata == nullptr)
		{
			return {};
		}

		return s_Data->AssetDirectory / metadata->Path;
	}

	const AssetRegistry& AssetManager::GetRegistry()
	{
		return s_Data != nullptr ? s_Data->Registry : s_EmptyRegistry;
	}
}
