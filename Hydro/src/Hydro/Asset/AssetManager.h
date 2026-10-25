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

	// Assets subsystem: knows every asset in the open project by handle (AssetRegistry), and keeps the
	// handles stable when files are renamed, moved or copied. Started after Renderer (it will create
	// GPU textures); the application tells it which project is open.
	//
	// Steps 1 and 2 of the asset system (docs/Architecture.md, "Asset system"). Identity: every texture,
	// scene, prefab and animation set file gets a handle. Loading: textures are loaded through the
	// manager, which keeps them (strong Refs) until the project closes.
	// Components hold handles; render and animation code resolve them with GetLoadedAsset every frame.
	//
	//   AssetManager::OnProjectOpened(project->GetAssetDirectory()); // scans the folder
	//   AssetHandle handle = AssetManager::FindHandle(path);       // 0 when not registered
	//   const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
	//   Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);        // loads if needed
	//   Ref<Texture2D> drawn = AssetManager::GetLoadedAsset<Texture2D>(handle);    // never loads
	class AssetManager
	{
	public:
		static void Init();
		static void Shutdown();

		// The application calls these when it opens or closes a project, like Renderer::SetSettings.
		// Opening scans `assetDirectory` (the project's asset folder), writing a handle for every asset
		// file that has none, and replaces the registry of any project opened before.
		static void OnProjectOpened(const std::filesystem::path& assetDirectory);
		static void OnProjectClosed();
		static bool IsProjectOpen();
		// Absolute; empty when no project is open.
		static const std::filesystem::path& GetAssetDirectory();

		// Scans the asset folder again, picking up files added, renamed, moved or copied since the last
		// scan (in the editor or outside it). A file keeps its handle wherever it is; a file without one
		// gets a new one; when two files have the same handle (one was copied), the one at the path
		// registered before keeps it (else the first path in alphabetical order) and the other gets a new
		// one. Registered files that are gone stay in the registry as missing. Synchronous: it reads the
		// first line of every scene and prefab and every sidecar. Does nothing when no project is open.
		static AssetScanInfo Rescan();

		// The handle for `file` (absolute, or relative to the asset folder), registering the file when it is
		// in the asset folder and isn't yet. Its stored handle is used when it has one (unless another file
		// already has it: then it is a copy and gets a new one); otherwise a new handle is made, and written
		// to the sidecar of an existing image. A scene or prefab that is about to be saved may not exist yet:
		// the serializer writes the returned handle into it. Returns 0 for files that aren't assets
		// (GetAssetTypeFromExtension) and for images that don't exist.
		static AssetHandle ImportAsset(const std::filesystem::path& file);

		// Renames or moves a file or folder inside the asset folder (`from` and `to` are absolute, or relative to
		// it) and keeps every handle: a file's own handle moves with it (a scene's first line, an image's
		// .hymeta, which is moved along), so everything that refers to it by handle follows. Fails (logged,
		// nothing changed) when `to` exists, when either path is outside the asset folder, or when a file's
		// extension would turn it into another kind of asset. A moved folder is scanned again.
		static bool MoveAsset(const std::filesystem::path& from, const std::filesystem::path& to);
		static bool MoveAsset(AssetHandle handle, const std::filesystem::path& to);

		// Deletes a file (with its .hymeta) or an empty folder inside the asset folder. The file's handle stays
		// in the registry, marked missing, so a scene that refers to it reports a missing asset instead of
		// quietly turning into "none"; its loaded object is dropped. Fails (logged) for a folder with files in it.
		static bool DeleteAsset(const std::filesystem::path& file);
		static bool DeleteAsset(AssetHandle handle);

		// Gives a loaded `asset` the handle of the file it was loaded from (ImportAsset). Loaders call this;
		// the handle stays 0 for files outside the asset folder.
		static void BindHandle(Asset& asset, const std::filesystem::path& file);

		// The handle of the registered file at `file` (absolute, or relative to the asset folder), or 0.
		static AssetHandle FindHandle(const std::filesystem::path& file);
		// nullptr when the handle isn't registered.
		static const AssetMetadata* GetMetadata(AssetHandle handle);
		// Registered and its file exists.
		static bool IsAssetHandleValid(AssetHandle handle);
		// Absolute path of a registered asset's file; empty when the handle isn't registered.
		static std::filesystem::path GetAbsolutePath(AssetHandle handle);

		static const AssetRegistry& GetRegistry();

		// The loaded asset for `handle`, loading it from its file when it isn't cached yet or its file
		// (or, for images, its .hymeta) changed or moved since. Synchronous, reads the disk: for scene
		// loading and the editor, never for code that runs every frame. nullptr (logged, once per file
		// version) when the handle is unknown, its file is missing or unreadable, or it isn't a T.
		// Texture2D, SpriteAnimationSet, Prefab and MaterialAsset can be loaded so far.
		template<typename T>
		static Ref<T> GetAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::GetAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(LoadAsset(handle, T::GetStaticType()));
		}

		// The asset if it is already loaded, else nullptr. Never touches the disk and never logs: the
		// only one render code calls. Pair it with GetAsset when a scene is loaded so it is loaded first.
		template<typename T>
		static Ref<T> GetLoadedAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::GetLoadedAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(FindLoadedAsset(handle, T::GetStaticType()));
		}

		// Loads the asset again now (settings or file changed) into the same handle, replacing the cached
		// one. Holders of the old object keep it. Returns the new object, nullptr on failure.
		template<typename T>
		static Ref<T> ReloadAsset(AssetHandle handle)
		{
			static_assert(std::is_base_of<Asset, T>::value, "AssetManager::ReloadAsset: T must derive from Asset");
			return std::static_pointer_cast<T>(LoadAsset(handle, T::GetStaticType(), true));
		}

		static AssetLoadState GetLoadState(AssetHandle handle);
		// Assets in the cache (loaded successfully).
		static size_t GetLoadedAssetCount();

	private:
		static Ref<Asset> LoadAsset(AssetHandle handle, AssetType type, bool force = false);
		static Ref<Asset> FindLoadedAsset(AssetHandle handle, AssetType type);
	};
}
