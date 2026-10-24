#pragma once
#include "Hydro/Renderer/TextureImport.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes an image's import config, "<image file>.hymeta" (YAML, see TextureImportConfig).
	// The same file holds the image's asset handle (AssetMetaSerializer): Serialize keeps its Handle and
	// Type keys, and WriteHandle keeps the TextureImport section.
	// TODO(assets): the import config becomes part of the texture's asset metadata with Roadmap AS2.
	class TextureImportSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymeta";

		static bool Serialize(const std::filesystem::path& filepath, const TextureImportConfig& config);
		// Missing keys keep their defaults. Returns false (and leaves `config` at defaults) when the
		// file can't be read (logged) or has no TextureImport section (not logged: only a handle).
		static bool Deserialize(const std::filesystem::path& filepath, TextureImportConfig& config);
	};
}
