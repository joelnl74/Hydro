#pragma once
#include "Hydro/Renderer/TextureImport.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	// Turns a sprite sheet into SpriteRegions. Functions on sizes and pixels (no GPU),
	// used by the editor when a texture's import config changes, and by tests.
	class SpriteSlicer
	{
	public:
		// Cuts a width x height image into cells as `slicing` describes, row by row from the top-left.
		// Cells that don't fit completely are left out. Sprite names are "<RowName>_<column>" when the
		// row has a name, otherwise "<baseName>_<index>". Every sprite gets slicing.DefaultPivot.
		static std::vector<SpriteRegion> SliceGrid(uint32_t width, uint32_t height, const SpriteSlicingConfig& slicing, const std::string& baseName);

		// Proposes a grid for a sheet of evenly spaced sprites: finds the runs of non-empty columns and
		// rows (alpha > 0, and not the colour key when there is one) and derives cell size, count and
		// offset from their spacing. `rgba` is width * height * 4 bytes, first row = top. Only the
		// grid fields of `slicing` (CellSize, Offset, Spacing, Columns, Rows) are written. Returns false
		// when the image is empty or holds a single sprite.
		static bool DetectGrid(const uint8_t* rgba, uint32_t width, uint32_t height, const TextureImportConfig& config, SpriteSlicingConfig& slicing);
		// The same for an image file (read with stb_image). False when it can't be read (logged).
		static bool DetectGrid(const std::filesystem::path& imagePath, const TextureImportConfig& config, SpriteSlicingConfig& slicing);
	};
}
