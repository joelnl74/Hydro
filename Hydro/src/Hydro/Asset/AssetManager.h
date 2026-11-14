#pragma once
#include "Hydro/Asset/AssetRegistry.h"
#include "Hydro/Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <type_traits>

namespace Hydro
{
	// What a scan of the asset folder found and fixed (also logged).
	struct AssetScanInfo
	{
		size_t AssetCount = 0;       // files registered, missing ones not included
		size_t NewHandles = 0;       // files without a handle that got one written
		size_t DuplicateHandles = 0; // files with another file's handle (a copy) that got a new one
		size_t MissingAssets = 0;    // registered earlier, file gone since
		size_t OrphanSidecars = 0;   // .hymeta files whose file is gone (moved without its sidecar?)
	};

	// Where an asset is in its life, for diagnostics (GetLoadState).
	enum class AssetLoadState : uint8_t
	{
		NotLoaded, // registered, nobody has asked for it yet (or the project was reopened)
		Loaded,    // in the cache
		Failed,    // its file couldn't be read or decoded (logged once); retried when the file changes
		Missing    // not in the registry, or its file is gone (a reference to it is dangling)
	};

	// Assets subsystem: every asset of the open project by handle, kept stable when files are renamed, moved
	// or copied, and the cache of loaded assets. See docs/Architecture.md, "Asset system".
	class AssetManager
	{
	public:
		static void Init();
		static void Shutdown();

		// Opening scans the asset folder, writes a handle for every asset file without one and replaces the
		// previous project's registry.
		static void OnProjectOpened(const std::filesystem::path& assetDirectory);
		static void OnProjectClosed();
		static bool IsProjectOpen();
		// Absolute; empty when no project is open.
		static const std::filesystem::path& GetAssetDirectory();

		// Scans the asset folder again for added, renamed, moved or copied files. A copy (a duplicate handle) gets
		// a new handle; files that are gone stay registered as missing. Synchronous.
		static AssetScanInfo Rescan();

		// The handle for `file`, registering it when it is in the asset folder (a copy gets a new handle). Works for
		// a scene or prefab not written yet; 0 for files that aren't assets and for images that don't exist.
		static AssetHandle ImportAsset(const std::filesystem::path& file);

		// Renames or moves a file or folder inside the asset folder, keeping every handle (an image's .hymeta moves
		// along). Fails (logged, nothing changed) when `to` exists, a path is outside the folder or the type changes.
		static bool MoveAsset(const std::filesystem::path& from, const std::filesystem::path& to);
		static bool MoveAsset(AssetHandle handle, const std::filesystem::path& to);

		// Deletes a file (with its .hymeta) or an empty folder. The handle stays registered as missing, so scenes
		// referring to it report a missing asset instead of quietly showing "none".
		static bool DeleteAsset(const std::filesystem::path& file);
		static bool DeleteAsset(AssetHandle handle);

		// Gives a loaded asset the handle of its file; stays 0 for files outside the asset folder. Loaders call this.
		static void BindHandle(Asset& asset, const std::filesystem::path& file);

		// `file` is absolute or relative to the asset folder; 0 when not registered.
		static AssetHandle FindHandle(const std::filesystem::path& file);
		static const AssetMetadata* GetMetadata(AssetHandle handle);
		static bool IsAssetHandleValid(AssetHandle handle);
		// Empty when the handle isn't registered.
		static std::filesystem::path GetAbsolutePath(AssetHandle handle);

		static const AssetRegistry& GetRegistry();

		// Loads the asset when it isn't cached or its file changed. Reads the disk: for scene loading and the
		// editor, never per frame. nullptr (logged once per file version) when unknown, missing, unreadable or not a T.
		template<typename T>
		static Ref<T> GetAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::GetAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(LoadAsset(handle, T::GetStaticType()));
		}

		// The asset if it is already loaded, else nullptr. Never touches the disk or logs: the one for render code.
		template<typename T>
		static Ref<T> GetLoadedAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::GetLoadedAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(FindLoadedAsset(handle, T::GetStaticType()));
		}

		// Loads the asset again into the same handle; holders of the old object keep it. nullptr on failure.
		template<typename T>
		static Ref<T> ReloadAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::ReloadAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(LoadAsset(handle, T::GetStaticType(), true));
		}

		// The editor just wrote the handle's file from its loaded object: record the new file version so the next
		// GetAsset doesn't reload the editor's own write.
		static void MarkSaved(AssetHandle handle);

		// An asset made in code (a texture a game fills) under a new handle that GetAsset/GetLoadedAsset return; no file:
		// never scanned, reloaded or saved. Kept until RemoveMemoryAsset or the project closes. 0 without a project.
		static AssetHandle AddMemoryAsset(const Ref<Asset>& asset);
		static void RemoveMemoryAsset(AssetHandle handle);
		static bool IsMemoryAsset(AssetHandle handle);

		static AssetLoadState GetLoadState(AssetHandle handle);
		// Loaded successfully.
		static size_t GetLoadedAssetCount();

	private:
		static Ref<Asset> LoadAsset(AssetHandle handle, AssetType type, bool force = false);
		static Ref<Asset> FindLoadedAsset(AssetHandle handle, AssetType type);
	};
}
