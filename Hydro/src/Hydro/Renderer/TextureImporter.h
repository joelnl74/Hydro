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

	// Decodes an image file (PNG, JPG, TGA, BMP; any channel count, converted to RGBA8) the way its
	// "<file>.hymeta" says: colour key, and slicing of a sheet that has slicing settings but no sprite
	// list. Never touches Vulkan, so it can run anywhere (a loader thread, later).
	class TextureImporter
	{
	public:
		// `fallback` is used when the file has no .hymeta (or it has no TextureImport section).
		// Returns false (logged) when the image can't be read or decoded.
		static bool Import(const std::filesystem::path& absolutePath, const TextureImportConfig& fallback, ImportedTexture& imported);

		// The .hymeta's modification time, 0 when the image has none: part of a texture's version, so
		// saving new import settings makes the texture load again.
		static int64_t GetImportConfigVersion(const std::filesystem::path& absolutePath);
	};
}
