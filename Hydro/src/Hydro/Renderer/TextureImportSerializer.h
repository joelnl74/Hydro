#pragma once
#include "Hydro/Renderer/TextureImport.h"

#include <filesystem>

namespace Hydro
{
	// Reads and writes the TextureImport section of an image's .hymeta, keeping the file's Handle and Type keys.
	// TODO(assets): the import config becomes part of the texture's asset metadata.
	class TextureImportSerializer
	{
	public:
		static constexpr const char* FileExtension = ".hymeta";

		static bool Serialize(const std::filesystem::path& filepath, const TextureImportConfig& config);
		// Missing keys keep their defaults. False (config at defaults) when the file can't be read (logged) or has no
		// TextureImport section (not logged).
		static bool Deserialize(const std::filesystem::path& filepath, TextureImportConfig& config);
	};
}
