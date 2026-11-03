#pragma once
#include "Hydro/Renderer/MeshImporter.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes a model's import settings and mapping in the MeshImport section of its .hymeta, keeping the
	// file's Handle and Type keys.
	class MeshImportSerializer
	{
	public:
		static bool Serialize(const std::filesystem::path& sidecar, const MeshImportConfig& config, const MeshImportMapping& mapping);
		// Missing keys keep their defaults. False when the file can't be read or has no MeshImport section.
		static bool Deserialize(const std::filesystem::path& sidecar, MeshImportConfig& config, MeshImportMapping& mapping);
	};
}
