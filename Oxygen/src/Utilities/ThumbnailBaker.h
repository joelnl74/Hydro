#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Hydro
{
	// A small copy of an image for the editor's tiles and lists.
	struct ThumbnailPixels
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<uint8_t> Pixels; // RGBA8, first row = top
		// Made smaller (draw it with linear sampling); otherwise the image as it is (pixel art stays crisp with nearest).
		bool Downscaled = false;
		// The image is a sprite sheet (sprite mode Multiple).
		bool IsSheet = false;
	};

	// Makes thumbnails and caches them in files, so the editor doesn't decode every image at each start. Pixels and
	// files only: no GPU, no engine types.
	class ThumbnailBaker
	{
	public:
		// The longest side of a thumbnail, in pixels.
		static constexpr uint32_t MaxSize = 128;

		// `rgba` is width * height RGBA8. A bigger image than MaxSize is averaged down to fit (colours weighted by alpha),
		// keeping its aspect ratio; a smaller one is copied.
		static ThumbnailPixels Downscale(const uint8_t* rgba, uint32_t width, uint32_t height);

		// Writes `thumbnail` to `file` (folders are created) with the version of the image it was made from.
		static bool Write(const std::filesystem::path& file, uint64_t sourceVersion, const ThumbnailPixels& thumbnail);
		// Reads it back. False when there is no file, it is damaged, or it was made from another version.
		static bool Read(const std::filesystem::path& file, uint64_t sourceVersion, ThumbnailPixels& thumbnail);
	};
}
