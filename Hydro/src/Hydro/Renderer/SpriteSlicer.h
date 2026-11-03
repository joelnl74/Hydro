#pragma once
#include "Hydro/Renderer/TextureImport.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Turns a sprite sheet into SpriteRegions. Sizes and pixels only (no GPU).
	class SpriteSlicer
	{
	public:
		// Cuts the image into whole cells as `slicing` says, row by row from the top-left. Names are "<RowName>_<column>"
		// for named rows, else "<baseName>_<index>".
		static std::vector<SpriteRegion> SliceGrid(uint32_t width, uint32_t height, const SpriteSlicingConfig& slicing,
			const std::string& baseName);
		// Proposes a grid for a sheet of evenly spaced sprites from its runs of non-empty columns and rows; writes only
		// the grid fields of `slicing`. False when the image is empty or holds a single sprite.
		static bool DetectGrid(const uint8_t* rgba, uint32_t width, uint32_t height, const TextureImportConfig& config,
			SpriteSlicingConfig& slicing);
		// The same for an image file (read with stb_image). False when it can't be read (logged).
		static bool DetectGrid(const std::filesystem::path& imagePath, const TextureImportConfig& config, SpriteSlicingConfig& slicing);
	};
}
