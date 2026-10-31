#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Hydro
{
	// Writes pixels to image files (stb_image_write). CPU only, no GPU. Folders in the path are created.
	class ImageWriter
	{
	public:
		// `rgba` is width * height RGBA8 pixels, rows top to bottom, tightly packed. The bytes are written
		// as they are, so sRGB-encoded pixels (an SRGB render target) give a correct PNG. Logs and returns
		// false when the file can't be written.
		static bool WritePNG(const std::filesystem::path& path, uint32_t width, uint32_t height, const uint8_t* rgba);
		// The same PNG in memory, appended to `out` (the terrain's weight map is embedded in its file). Fixed
		// settings, so the same pixels always give the same bytes. False when encoding failed.
		static bool EncodePNG(uint32_t width, uint32_t height, const uint8_t* rgba, std::vector<uint8_t>& out);
	};
}
