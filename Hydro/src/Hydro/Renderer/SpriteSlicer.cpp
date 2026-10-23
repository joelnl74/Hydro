#include "hypch.h"
#include "SpriteSlicer.h"

#include <cmath>

#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#include <stb_image.h> // implementation compiled in Texture2D.cpp
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace Hydro
{
	struct SpriteAxisGrid
	{
		uint32_t Count = 0;
		uint32_t Cell = 0;
		uint32_t Offset = 0;
	};

	// Turns "is anything drawn at this column/row" into a grid along one axis: the runs of occupied
	// lines, with small gaps (holes inside a sprite) merged, spaced evenly.
	static SpriteAxisGrid DetectAxis(const std::vector<bool>& occupied)
	{
		struct Run
		{
			uint32_t Start = 0;
			uint32_t End = 0; // inclusive
		};

		std::vector<Run> runs;
		for (uint32_t i = 0; i < (uint32_t)occupied.size(); i++)
		{
			if (occupied[i] == false)
			{
				continue;
			}
			if (runs.empty() == false && runs.back().End + 1 == i)
			{
				runs.back().End = i;
			}
			else
			{
				runs.push_back({ i, i });
			}
		}

		const uint32_t size = (uint32_t)occupied.size();
		SpriteAxisGrid grid;
		if (runs.size() < 2)
		{
			// One sprite (or nothing) along this axis: one cell spanning the image.
			grid.Count = runs.empty() ? 0 : 1;
			grid.Cell = size;
			return grid;
		}

		// Gaps between sprites are the large ones; a gap under a third of the largest is a hole
		// inside a sprite (between legs, say), so those runs are joined.
		uint32_t largestGap = 0;
		for (size_t i = 1; i < runs.size(); i++)
		{
			largestGap = std::max(largestGap, runs[i].Start - runs[i - 1].End - 1);
		}

		std::vector<Run> merged;
		merged.push_back(runs[0]);
		for (size_t i = 1; i < runs.size(); i++)
		{
			const uint32_t gap = runs[i].Start - merged.back().End - 1;
			if (gap * 3 < largestGap)
			{
				merged.back().End = runs[i].End;
			}
			else
			{
				merged.push_back(runs[i]);
			}
		}

		grid.Count = (uint32_t)merged.size();
		if (grid.Count < 2)
		{
			grid.Count = 1;
			grid.Cell = size;
			return grid;
		}

		// Even spacing from the first and last sprite's centres, so sprites of different widths
		// still give the right step.
		const float firstCentre = 0.5f * (float)(merged.front().Start + merged.front().End + 1);
		const float lastCentre = 0.5f * (float)(merged.back().Start + merged.back().End + 1);
		grid.Cell = std::max(1u, (uint32_t)std::lround((lastCentre - firstCentre) / (float)(grid.Count - 1)));

		// The first cell is centred on the first sprite, but never so far in that the last cell
		// falls off the image (sheets usually start at 0 with sprites not centred in their cells).
		const float centredOffset = firstCentre - 0.5f * (float)grid.Cell;
		uint32_t offset = centredOffset > 0.0f ? (uint32_t)std::lround(centredOffset) : 0u;
		const uint64_t gridSize = (uint64_t)grid.Count * grid.Cell;
		if (gridSize <= size)
		{
			offset = std::min(offset, size - (uint32_t)gridSize);
		}
		grid.Offset = offset;
		return grid;
	}

	std::vector<SpriteRegion> SpriteSlicer::SliceGrid(uint32_t width, uint32_t height, const SpriteSlicingConfig& slicing, const std::string& baseName)
	{
		std::vector<SpriteRegion> sprites;
		if (slicing.CellSize.x == 0 || slicing.CellSize.y == 0)
		{
			return sprites;
		}

		const glm::uvec2 step = slicing.CellSize + slicing.Spacing;

		// How many whole cells fit from the offset, limited by Columns/Rows when they are set.
		const auto fitting = [](uint32_t imageSize, uint32_t offset, uint32_t cell, uint32_t stride, uint32_t limit)
		{
			uint32_t count = 0;
			if (offset + cell <= imageSize)
			{
				count = 1 + (imageSize - offset - cell) / stride;
			}
			return limit > 0 ? std::min(count, limit) : count;
		};

		const uint32_t columns = fitting(width, slicing.Offset.x, slicing.CellSize.x, step.x, slicing.Columns);
		const uint32_t rows = fitting(height, slicing.Offset.y, slicing.CellSize.y, step.y, slicing.Rows);

		sprites.reserve((size_t)columns * rows);
		for (uint32_t row = 0; row < rows; row++)
		{
			const bool rowHasName = row < slicing.RowNames.size() && slicing.RowNames[row].empty() == false;
			for (uint32_t column = 0; column < columns; column++)
			{
				SpriteRegion sprite;
				sprite.Position = slicing.Offset + glm::uvec2(column, row) * step;
				sprite.Size = slicing.CellSize;
				sprite.Pivot = slicing.DefaultPivot;
				if (rowHasName)
				{
					sprite.Name = slicing.RowNames[row] + "_" + std::to_string(column);
				}
				else
				{
					sprite.Name = baseName + "_" + std::to_string(row * columns + column);
				}
				sprites.push_back(std::move(sprite));
			}
		}

		return sprites;
	}

	bool SpriteSlicer::DetectGrid(const uint8_t* rgba, uint32_t width, uint32_t height, const TextureImportConfig& config, SpriteSlicingConfig& slicing)
	{
		if (rgba == nullptr || width == 0 || height == 0)
		{
			return false;
		}

		std::vector<bool> occupiedColumns(width, false);
		std::vector<bool> occupiedRows(height, false);
		for (uint32_t y = 0; y < height; y++)
		{
			const uint8_t* pixel = rgba + (size_t)y * width * 4;
			for (uint32_t x = 0; x < width; x++, pixel += 4)
			{
				bool empty = pixel[3] == 0;
				if (empty == false && config.UseColorKey)
				{
					empty = pixel[0] == config.ColorKey.r && pixel[1] == config.ColorKey.g && pixel[2] == config.ColorKey.b;
				}
				if (empty == false)
				{
					occupiedColumns[x] = true;
					occupiedRows[y] = true;
				}
			}
		}

		const SpriteAxisGrid columns = DetectAxis(occupiedColumns);
		const SpriteAxisGrid rows = DetectAxis(occupiedRows);
		if (columns.Count == 0 || rows.Count == 0 || (columns.Count == 1 && rows.Count == 1))
		{
			return false;
		}

		slicing.CellSize = { columns.Cell, rows.Cell };
		slicing.Offset = { columns.Offset, rows.Offset };
		slicing.Spacing = { 0, 0 };
		slicing.Columns = columns.Count;
		slicing.Rows = rows.Count;
		return true;
	}

	bool SpriteSlicer::DetectGrid(const std::filesystem::path& imagePath, const TextureImportConfig& config, SpriteSlicingConfig& slicing)
	{
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load(imagePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (pixels == nullptr)
		{
			HY_CORE_ERROR("SpriteSlicer: can't read '{}': {}", imagePath.string(), stbi_failure_reason());
			return false;
		}

		const bool found = DetectGrid(pixels, (uint32_t)width, (uint32_t)height, config, slicing);
		stbi_image_free(pixels);
		return found;
	}
}
