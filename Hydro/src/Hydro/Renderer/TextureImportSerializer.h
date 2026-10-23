#pragma once
#include "Hydro/Renderer/TextureImport.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes an image's import config, "<image file>.hymeta" (YAML, see TextureImportConfig).
	// TODO(assets): moves into the asset registry with Roadmap AS2.
	class TextureImportSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymeta";

		static bool Serialize(const std::filesystem::path& filepath, const TextureImportConfig& config);
		// Missing keys keep their defaults. Returns false (and leaves `config` at defaults) when the
		// file can't be read or isn't a texture import file.
		static bool Deserialize(const std::filesystem::path& filepath, TextureImportConfig& config);
	};
}
