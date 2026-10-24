#pragma once
#include "Hydro/Asset/AssetRegistry.h"

#include <cstddef>
#include <filesystem>

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

	// Assets subsystem: knows every asset in the open project by handle (AssetRegistry), and keeps the
	// handles stable when files are renamed, moved or copied. Started after Renderer (it will create
	// GPU textures); the application tells it which project is open.
	//
	// Step 1 of the asset system (docs/Architecture.md, "Asset system"): identity only. Every texture,
	// scene and prefab file gets a handle, but nothing is loaded through the manager yet; sprites still
	// hold a Ref<Texture2D> and scenes save texture paths. TODO(assets): GetAsset<T>(handle) and
	// GetLoadedAsset<T>(handle) come with the texture step.
	//
	//   AssetManager::OnProjectOpened(project->GetAssetDirectory()); // scans the folder
	//   AssetHandle handle = AssetManager::FindHandle(path);       // 0 when not registered
	//   const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
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

		// The handle of the registered file at `file` (absolute, or relative to the asset folder), or 0.
		static AssetHandle FindHandle(const std::filesystem::path& file);
		// nullptr when the handle isn't registered.
		static const AssetMetadata* GetMetadata(AssetHandle handle);
		// Registered and its file exists.
		static bool IsAssetHandleValid(AssetHandle handle);
		// Absolute path of a registered asset's file; empty when the handle isn't registered.
		static std::filesystem::path GetAbsolutePath(AssetHandle handle);

		static const AssetRegistry& GetRegistry();
	};
}
