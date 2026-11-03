#pragma once
#include "Hydro/Renderer/Image.h"
#include "Hydro/Renderer/TextureImport.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Hydro
{
	// An image file decoded and ready to upload: what Texture2D::Create needs. Plain data, no GPU.
	struct ImportedTexture
	{
		ImageSpecification Specification;   // Width, Height, Format, Usage = Texture
		std::vector<uint8_t> Pixels;        // Width * Height * 4 bytes (RGBA8, first row = top), colour key applied
		TextureImportConfig Config;         // what the file's .hymeta (or the fallback) asked for, sliced when needed
	};

	// Decodes an image file into RGBA8 as its .hymeta says (colour key, slicing). No Vulkan, so it can run anywhere.
	class TextureImporter
	{
	public:
		// `fallback` is used when the file has no .hymeta. False (logged) when the image can't be read or decoded.
		static bool Import(const std::filesystem::path& absolutePath, const TextureImportConfig& fallback, ImportedTexture& imported);

		// The .hymeta's modification time (0 without one): part of a texture's version, so new settings reload it.
		static int64_t GetImportConfigVersion(const std::filesystem::path& absolutePath);
	};
}
