#include "hypch.h"
#include "AssetManager.h"
#include "AssetMetaSerializer.h"

#include "Hydro/Renderer/TextureImporter.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Prefab.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	// One cache entry: the loaded object (null when loading failed) and what it was loaded from, so a
	// later request can tell the file changed or moved.
	struct LoadedAsset
	{
		Ref<Asset> Object;
		AssetType Type = AssetType::None;
		std::string Path;      // registry path (forward slashes) it was loaded from
		uint64_t Version = 0;  // file + .hymeta modification times
		bool Failed = false;
	};

	struct AssetManagerData
	{
		std::filesystem::path AssetDirectory; // absolute, normalised; empty = no project
		AssetRegistry Registry;
		std::unordered_map<AssetHandle, LoadedAsset> Loaded;
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
		s_Data->Loaded.clear();

		Rescan();
	}

	void AssetManager::OnProjectClosed()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// Frees the loaded assets (their GPU images go through the renderer's deletion queue); objects
		// still held elsewhere live on until their holders let go.
		s_Data->Loaded.clear();
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

		// Assets whose file is gone are no longer kept. (A moved file keeps its cache entry: the next
		// GetAsset sees its path changed and loads it again from the new place.)
		for (auto it = s_Data->Loaded.begin(); it != s_Data->Loaded.end();)
		{
			it = IsAssetHandleValid(it->first) ? std::next(it) : s_Data->Loaded.erase(it);
		}

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

	bool AssetManager::MoveAsset(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		if (IsProjectOpen() == false)
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: no project is open");
			return false;
		}

		const std::filesystem::path relativeFrom = ToRelativePath(from);
		const std::filesystem::path relativeTo = ToRelativePath(to);
		if (relativeFrom.empty() || relativeTo.empty())
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: '{}' to '{}': both must be inside the asset folder", from.string(), to.string());
			return false;
		}

		const std::filesystem::path absoluteFrom = s_Data->AssetDirectory / relativeFrom;
		const std::filesystem::path absoluteTo = s_Data->AssetDirectory / relativeTo;

		std::error_code ec;
		if (std::filesystem::exists(absoluteFrom, ec) == false)
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: '{}' doesn't exist", relativeFrom.generic_u8string());
			return false;
		}
		if (std::filesystem::exists(absoluteTo, ec))
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: '{}' already exists", relativeTo.generic_u8string());
			return false;
		}

		if (std::filesystem::is_directory(absoluteFrom, ec))
		{
			// Into itself: the file system would refuse it, but with a worse message.
			const std::filesystem::path inside = relativeTo.lexically_relative(relativeFrom);
			if (inside.empty() == false && *inside.begin() != "..")
			{
				HY_CORE_ERROR("AssetManager::MoveAsset: can't move '{}' into itself", relativeFrom.generic_u8string());
				return false;
			}

			std::filesystem::rename(absoluteFrom, absoluteTo, ec);
			if (ec)
			{
				HY_CORE_ERROR("AssetManager::MoveAsset: '{}': {}", relativeFrom.generic_u8string(), ec.message());
				return false;
			}

			// Every file in it kept its handle, so a scan finds them again at their new paths.
			Rescan();
			return true;
		}

		const AssetType type = GetAssetTypeFromExtension(absoluteFrom);
		if (type != GetAssetTypeFromExtension(absoluteTo))
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: '{}' to '{}' would change what kind of asset it is",
				relativeFrom.generic_u8string(), relativeTo.generic_u8string());
			return false;
		}

		std::filesystem::rename(absoluteFrom, absoluteTo, ec);
		if (ec)
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: '{}': {}", relativeFrom.generic_u8string(), ec.message());
			return false;
		}

		// An image's .hymeta holds its handle and import settings; without it the image would get a new handle.
		const std::filesystem::path sidecarFrom = AssetMetaSerializer::GetSidecarPath(absoluteFrom);
		if (std::filesystem::exists(sidecarFrom, ec))
		{
			std::filesystem::rename(sidecarFrom, AssetMetaSerializer::GetSidecarPath(absoluteTo), ec);
			if (ec)
			{
				HY_CORE_ERROR("AssetManager::MoveAsset: couldn't move '{}': {}; undoing the move", sidecarFrom.string(), ec.message());
				std::filesystem::rename(absoluteTo, absoluteFrom, ec);
				return false;
			}
		}

		const AssetHandle handle = s_Data->Registry.Find(relativeFrom);
		if (const AssetMetadata* registered = s_Data->Registry.Get(handle))
		{
			AssetMetadata metadata = *registered;
			metadata.Path = relativeTo;
			s_Data->Registry.Add(metadata);
		}

		return true;
	}

	bool AssetManager::MoveAsset(AssetHandle handle, const std::filesystem::path& to)
	{
		const AssetMetadata* metadata = GetMetadata(handle);
		if (metadata == nullptr || metadata->IsMissing)
		{
			HY_CORE_ERROR("AssetManager::MoveAsset: asset handle {} is not in the project", (uint64_t)handle);
			return false;
		}

		return MoveAsset(s_Data->AssetDirectory / metadata->Path, to);
	}

	bool AssetManager::DeleteAsset(const std::filesystem::path& file)
	{
		if (IsProjectOpen() == false)
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: no project is open");
			return false;
		}

		const std::filesystem::path relative = ToRelativePath(file);
		if (relative.empty())
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: '{}' is not inside the asset folder", file.string());
			return false;
		}

		const std::filesystem::path absolute = s_Data->AssetDirectory / relative;
		std::error_code ec;
		if (std::filesystem::exists(absolute, ec) == false)
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: '{}' doesn't exist", relative.generic_u8string());
			return false;
		}

		const bool isFolder = std::filesystem::is_directory(absolute, ec);
		if (isFolder && std::filesystem::is_empty(absolute, ec) == false)
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: folder '{}' isn't empty", relative.generic_u8string());
			return false;
		}

		if (std::filesystem::remove(absolute, ec) == false)
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: '{}': {}", relative.generic_u8string(), ec.message());
			return false;
		}

		if (isFolder)
		{
			return true;
		}

		// An image's handle lives next to it; without the image it means nothing.
		std::filesystem::remove(AssetMetaSerializer::GetSidecarPath(absolute), ec);

		const AssetHandle handle = s_Data->Registry.Find(relative);
		if (const AssetMetadata* registered = s_Data->Registry.Get(handle))
		{
			AssetMetadata missing = *registered;
			missing.IsMissing = true;
			s_Data->Registry.Add(missing);
		}
		s_Data->Loaded.erase(handle);
		return true;
	}

	bool AssetManager::DeleteAsset(AssetHandle handle)
	{
		const AssetMetadata* metadata = GetMetadata(handle);
		if (metadata == nullptr || metadata->IsMissing)
		{
			HY_CORE_ERROR("AssetManager::DeleteAsset: asset handle {} is not in the project", (uint64_t)handle);
			return false;
		}

		return DeleteAsset(s_Data->AssetDirectory / metadata->Path);
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

	// The version of what an asset is loaded from: its file's and (for images) its .hymeta's modification
	// times, so saving either one makes the next GetAsset load again.
	static uint64_t GetSourceVersion(const std::filesystem::path& absolute, AssetType type)
	{
		std::error_code ec;
		uint64_t version = (uint64_t)std::filesystem::last_write_time(absolute, ec).time_since_epoch().count();
		if (AssetMetaSerializer::StoresHandleInFile(type) == false)
		{
			const std::filesystem::path sidecar = AssetMetaSerializer::GetSidecarPath(absolute);
			const uint64_t sidecarVersion = std::filesystem::exists(sidecar, ec) ?
				(uint64_t)std::filesystem::last_write_time(sidecar, ec).time_since_epoch().count() : 0;
			version = version * 1000003ull + sidecarVersion;
		}
		return version;
	}

	// Decodes and creates the object for one asset file. nullptr (logged) when it can't be.
	static Ref<Asset> LoadFromDisk(const std::filesystem::path& absolute, AssetType type)
	{
		switch (type)
		{
			case AssetType::Texture2D:
			{
				ImportedTexture imported;
				if (TextureImporter::Import(absolute, TextureImportConfig{}, imported) == false)
				{
					return nullptr;
				}
				return Texture2D::Create(imported, absolute);
			}
			case AssetType::SpriteAnimation:
			{
				// Reads its texture through Texture2D::LoadFromFile, i.e. this manager.
				return SpriteAnimationSet::LoadUncached(absolute);
			}
			case AssetType::Prefab:
			{
				return Prefab::LoadUncached(absolute);
			}
			default:
				HY_CORE_ERROR("AssetManager: assets of type {} can't be loaded through the manager yet ('{}')",
					AssetTypeToString(type), absolute.string());
				return nullptr;
		}
	}

	Ref<Asset> AssetManager::LoadAsset(AssetHandle handle, AssetType type, bool force)
	{
		if (s_Data == nullptr || (uint64_t)handle == 0)
		{
			return nullptr;
		}

		const AssetMetadata* metadata = s_Data->Registry.Get(handle);
		if (metadata == nullptr)
		{
			HY_CORE_WARN("AssetManager: asset handle {} is not in the project (deleted, or from another project?)", (uint64_t)handle);
			return nullptr;
		}
		if (metadata->IsMissing)
		{
			HY_CORE_WARN("AssetManager: the file of asset '{}' is missing", metadata->Path.generic_u8string());
			return nullptr;
		}
		if (metadata->Type != type)
		{
			HY_CORE_ERROR("AssetManager: '{}' is a {}, not a {}", metadata->Path.generic_u8string(),
				AssetTypeToString(metadata->Type), AssetTypeToString(type));
			return nullptr;
		}

		const std::filesystem::path absolute = s_Data->AssetDirectory / metadata->Path;
		const std::string pathKey = AssetRegistry::MakePathKey(metadata->Path);
		const uint64_t version = GetSourceVersion(absolute, type);

		auto found = s_Data->Loaded.find(handle);
		if (force == false && found != s_Data->Loaded.end() && found->second.Version == version && found->second.Path == pathKey)
		{
			return found->second.Object; // null for a failed load that hasn't changed: already logged
		}

		LoadedAsset entry;
		entry.Object = LoadFromDisk(absolute, type);
		entry.Type = type;
		entry.Path = pathKey;
		entry.Version = version;
		entry.Failed = entry.Object == nullptr;
		if (entry.Object != nullptr)
		{
			entry.Object->m_Handle = handle;
		}
		else
		{
			// A file that was fine and is now broken (a save in progress, a bad edit): keep showing the
			// last good object. The state says Failed until the file is fixed. (Looked up again: loading
			// may have loaded other assets, such as the texture of an animation set, and moved the table.)
			auto previous = s_Data->Loaded.find(handle);
			if (previous != s_Data->Loaded.end())
			{
				entry.Object = previous->second.Object;
			}
		}

		Ref<Asset> result = entry.Object;
		s_Data->Loaded[handle] = std::move(entry);
		return result;
	}

	Ref<Asset> AssetManager::FindLoadedAsset(AssetHandle handle, AssetType type)
	{
		if (s_Data == nullptr)
		{
			return nullptr;
		}

		auto found = s_Data->Loaded.find(handle);
		if (found == s_Data->Loaded.end() || found->second.Type != type)
		{
			return nullptr;
		}

		return found->second.Object;
	}

	AssetLoadState AssetManager::GetLoadState(AssetHandle handle)
	{
		if (IsAssetHandleValid(handle) == false)
		{
			return AssetLoadState::Missing;
		}

		auto found = s_Data->Loaded.find(handle);
		if (found == s_Data->Loaded.end())
		{
			return AssetLoadState::NotLoaded;
		}

		return found->second.Failed ? AssetLoadState::Failed : AssetLoadState::Loaded;
	}

	size_t AssetManager::GetLoadedAssetCount()
	{
		if (s_Data == nullptr)
		{
			return 0;
		}

		size_t count = 0;
		for (const auto& [handle, entry] : s_Data->Loaded)
		{
			if (entry.Object != nullptr)
			{
				count++;
			}
		}
		return count;
	}

	const AssetRegistry& AssetManager::GetRegistry()
	{
		return s_Data != nullptr ? s_Data->Registry : s_EmptyRegistry;
	}
}
