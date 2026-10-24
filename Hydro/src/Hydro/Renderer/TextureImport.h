#pragma once
#include "Hydro/Renderer/Image.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// Whether a texture is one sprite (the whole image) or a sheet sliced into many named sprites.
	enum class SpriteMode : uint8_t
	{
		Single = 0,
		Multiple
	};

	// How the sprites of a Multiple texture were produced. Only the editor uses it (to slice again);
	// the runtime reads TextureImportConfig::Sprites.
	enum class SpriteSlicingMode : uint8_t
	{
		Manual = 0, // rectangles placed by hand
		Grid,       // cells of CellSize, see SpriteSlicingConfig
		Auto        // a grid proposed by SpriteSlicer::DetectGrid, then sliced like Grid
	};

	// One sprite inside a texture, in pixels with (0, 0) at the image's top-left corner.
	struct SpriteRegion
	{
		std::string Name;
		glm::uvec2 Position{ 0 };
		glm::uvec2 Size{ 0 };
		// The point the sprite is placed by, as a fraction of its size: (0.5, 0.5) is the centre,
		// (0.5, 0) the bottom centre (feet).
		glm::vec2 Pivot{ 0.5f, 0.5f };
	};

	struct SpriteSlicingConfig
	{
		SpriteSlicingMode Mode = SpriteSlicingMode::Grid;
		glm::uvec2 CellSize{ 32, 32 };
		glm::uvec2 Offset{ 0 };     // top-left of the first cell
		glm::uvec2 Spacing{ 0 };    // gap between cells
		uint32_t Columns = 0;       // 0 = as many as fit
		uint32_t Rows = 0;          // 0 = as many as fit
		// Sprite names are "<RowName>_<column>"; rows without a name use "<texture name>_<index>".
		std::vector<std::string> RowNames;
		glm::vec2 DefaultPivot{ 0.5f, 0.5f };
	};

	// How an image file is imported: saved next to it as "<file>.hymeta" (TextureImportSerializer) and
	// read by Texture2D::LoadFromFile. Files without one import with these defaults.
	// TODO(assets): becomes the texture's entry in the asset registry (Roadmap AS2).
	struct TextureImportConfig
	{
		ImageFormat Format = ImageFormat::SRGB;   // SRGB for colour, RGBA for data
		TextureProperties Properties;             // filter, wrap
		// Pixels of exactly this colour become transparent (for sheets exported without alpha).
		bool UseColorKey = false;
		glm::uvec3 ColorKey{ 0 };                 // 0..255 per channel

		SpriteMode Mode = SpriteMode::Single;
		float PixelsPerUnit = 32.0f;
		SpriteSlicingConfig Slicing;
		// Multiple only. When it is empty, Texture2D::LoadFromFile slices the image with Slicing
		// (Grid, or Auto: grid detected first). Single textures have one sprite: the whole image.
		std::vector<SpriteRegion> Sprites;
	};
}
