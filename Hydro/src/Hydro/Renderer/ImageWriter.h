#pragma once
#include <cstdint>
#include <filesystem>

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
	};
}
