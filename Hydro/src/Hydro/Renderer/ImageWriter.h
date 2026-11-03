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
		// `rgba` is tightly packed RGBA8, rows top to bottom, written as is (sRGB pixels give a correct PNG).
		// False (logged) when the file can't be written.
		static bool WritePNG(const std::filesystem::path& path, uint32_t width, uint32_t height, const uint8_t* rgba);
		// The same PNG appended to `out`. Fixed settings, so the same pixels always give the same bytes.
		static bool EncodePNG(uint32_t width, uint32_t height, const uint8_t* rgba, std::vector<uint8_t>& out);
	};
}
