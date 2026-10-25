#pragma once
#include "Hydro/Core/UUID.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Hydro
{
	// Names one asset (a file in the project's asset folder) for as long as that file exists, across
	// renames and moves. 0 means "no asset". Components and saved files store handles, never paths.
	//
	// Where a file keeps its handle (AssetMetaSerializer): files Hydro writes (scenes, prefabs) have a
	// "Handle:" line at the top; other files (images) have a "<file>.hymeta" sidecar next to them.
	// The AssetManager scans the asset folder when a project opens and builds an AssetRegistry from them.
	using AssetHandle = UUID;

	// What kind of file an asset is. Saved by name in .hymeta files, so add new types at the end and
	// keep the names stable (AssetTypeToString).
	enum class AssetType : uint16_t
	{
		None = 0,
		Texture2D,
		Scene,
		Prefab,
		SpriteAnimation,
		Material
	};

	// "Texture2D", "Scene", ... ("None" for AssetType::None).
	const char* AssetTypeToString(AssetType type);
	// The inverse of AssetTypeToString; AssetType::None for an unknown name.
	AssetType AssetTypeFromString(const std::string& name);
	// The asset type of a file, from its extension (case-insensitive): .png/.jpg/.jpeg/.tga/.bmp ->
	// Texture2D, .hyscene -> Scene, .hyprefab -> Prefab, .hyanim -> SpriteAnimation, .hymat -> Material. AssetType::None for anything else, including
	// .hymeta sidecars and folders.
	AssetType GetAssetTypeFromExtension(const std::filesystem::path& path);

	// Base of the asset types the AssetManager will load and hand out as Ref<T> (Texture2D now; fonts,
	// audio clips and a Prefab later). Holds the handle and nothing else: no file paths, no Vulkan.
	// Not every AssetType is a loaded object: a scene is an asset (a file with a handle), but opening
	// it always builds a fresh Scene, which is not shared or cached.
	//
	// The handle is set when the object is loaded from a file inside the asset folder (the loader calls
	// AssetManager::BindHandle) and stays 0 otherwise (made in code, or loaded from outside the project).
	// TODO(assets): the AssetManager does the loading itself (GetAsset<T>) in the texture step.
	class Asset
	{
	public:
		virtual ~Asset() = default;

		AssetHandle GetHandle() const { return m_Handle; }
		virtual AssetType GetType() const = 0;

	private:
		AssetHandle m_Handle = 0;

		friend class AssetManager;
	};
}
