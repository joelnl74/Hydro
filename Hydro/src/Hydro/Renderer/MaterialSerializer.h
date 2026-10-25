#pragma once
#include "Hydro/Renderer/Material.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes .hymat files: one Standard material (YAML). The handle goes first, like in scenes
	// and .hyanim files (AssetMetaSerializer), so a rename or move keeps it.
	//
	//   Handle: 1234567890
	//   Material:
	//     Type: Standard
	//     AlbedoHandle: 345836454132741581   # a texture's handle; left out = white
	//     Tint: [1, 1, 1, 1]
	//     Tiling: [1, 1]
	//     EmissionColor: [0, 0, 0]
	//     EmissionIntensity: 1
	//     AlphaCutoff: 0
	//     Filter: Nearest                     # Nearest | Linear
	//
	// The same fields (without Type) are what a MeshComponent's material slot saves inline (EntitySerializer).
	class MaterialSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymat";

		// Writes `data` to `filepath` (a new file gets a handle; an existing one keeps its own). Returns
		// false (logged) when the file can't be written.
		static bool Serialize(const std::filesystem::path& filepath, const MaterialData& data);
		// Reads a .hymat file and loads its textures (AssetManager::GetAsset), so render code finds them.
		// Returns false (logged) when the file can't be read or isn't a material.
		static bool Deserialize(const std::filesystem::path& filepath, MaterialData& data);

		// The fields of one material as a YAML map (for files and inline slots), and back. Reading loads the
		// textures. Fields that are missing keep their defaults.
		static void SerializeData(YAML::Emitter& out, const MaterialData& data);
		static MaterialData DeserializeData(const YAML::Node& node);
	};
}
