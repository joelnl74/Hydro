#pragma once
#include "Hydro/Core/UUID.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Hydro
{
	// Names one asset file across renames and moves; 0 = no asset. Kept in a file's "Handle:" line or its .hymeta
	// sidecar (AssetMetaSerializer). Components and saved files store handles, never paths.
	using AssetHandle = UUID;

	// Saved by name in .hymeta files: add new types at the end and keep the names stable.
	enum class AssetType : uint16_t
	{
		None = 0,
		Texture2D,
		Scene,
		Prefab,
		SpriteAnimation,
		Material,
		Mesh,
		RenderProfile,
		Font,
		Terrain,
		Definition, // .hydef: one record of an HY_DATA type (Hydro/Data/DefinitionAsset.h)
		DataTable,  // .hytable: rows of an HY_DATA type by ID (Hydro/Data/DataTableAsset.h)
		// .frag: a material's fragment shader (MaterialData::Shader). Only registered: Renderer3D compiles it from its file,
		// so it is never loaded through the AssetManager.
		Shader
	};

	// "None" for AssetType::None.
	const char* AssetTypeToString(AssetType type);
	// AssetType::None for an unknown name.
	AssetType AssetTypeFromString(const std::string& name);
	// From the extension (case-insensitive); AssetType::None for anything else, including .hymeta sidecars and folders.
	AssetType GetAssetTypeFromExtension(const std::filesystem::path& path);

	// Base of the assets the AssetManager loads and hands out as Ref<T>: holds only the handle (0 when made in code or
	// loaded from outside the project). Scenes are assets too, but opening one always builds a fresh Scene.
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
