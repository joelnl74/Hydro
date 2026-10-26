#pragma once
#include "Hydro/Renderer/MeshImporter.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes a model's import settings and mapping in its "<model>.hymeta" (YAML). The same file
	// holds the model's asset handle (AssetMetaSerializer): Serialize keeps its Handle and Type keys.
	//
	//   Handle: 1234567890
	//   Type: Mesh
	//   MeshImport:
	//     Scale: 1
	//     ConvertYUpToZUp: true
	//     GenerateNormals: true
	//     Images: { 0: 99887766 }     # glTF image index -> texture handle
	//     Materials: { 0: 55443322 }  # glTF material index -> .hymat handle
	class MeshImportSerializer
	{
	public:
		static bool Serialize(const std::filesystem::path& sidecar, const MeshImportConfig& config, const MeshImportMapping& mapping);
		// Missing keys keep their defaults. False when the file can't be read or has no MeshImport section.
		static bool Deserialize(const std::filesystem::path& sidecar, MeshImportConfig& config, MeshImportMapping& mapping);
	};
}
