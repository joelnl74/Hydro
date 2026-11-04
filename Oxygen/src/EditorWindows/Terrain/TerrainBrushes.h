#pragma once
#include <Hydro/Terrain/Terrain.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The Terrain tool's brush settings, for this editor session only.
	struct TerrainBrushSettings
	{
		uint32_t Layer = 1;       // Paint and Path: the layer
		uint8_t WallLayer = 0;    // Wall: 0..3 or TerrainWallHidden
		float Radius = 1.0f;      // metres
		float Strength = 0.5f;    // Paint: how fast a dab reaches full weight (0..1)
		float Hardness = 0.5f;    // Paint: share of the radius at full strength; the rest fades out
		float PathWidth = 1.6f;   // Path: metres
		float PathWander = 0.15f; // Path: how far the edges wander in and out, metres
	};

	// The Terrain tool's edits of a terrain's data, one dab at a time; each marks the cells or weights it changed.
	// Positions are in the terrain's space, metres.
	class TerrainBrushes
	{
	public:
		// Raises brush.Layer in the circle at `centre` for `deltaTime` seconds, or lowers it. `weights` are the stroke's
		// weights as floats, four per texel, since bytes would lose small dabs.
		static void Paint(Terrain& terrain, const TerrainBrushSettings& brush, std::vector<float>& weights, const glm::vec2& centre,
			bool lower, float deltaTime);
		// Raise / Lower (`step` +1 or -1; each corner once per stroke, remembered in `touched`) or every corner to
		// `flattenLevel` (`flatten`).
		static void ChangeLevels(Terrain& terrain, const TerrainBrushSettings& brush, std::vector<uint8_t>& touched,
			const glm::vec2& centre, int32_t step, bool flatten, int8_t flattenLevel);
		static void PaintWalls(Terrain& terrain, const TerrainBrushSettings& brush, const glm::vec2& centre);
		// Gives the cells under the brush the direction from `strokeStart` to `centre` (its main axis), or none.
		static void PaintRamps(Terrain& terrain, const TerrainBrushSettings& brush, const glm::vec2& strokeStart,
			const glm::vec2& centre, bool clear);
		// Paints brush.Layer along a curve through `pathPoints`, with edges that wander in and out.
		static void PaintPath(Terrain& terrain, const TerrainBrushSettings& brush, const std::vector<glm::vec3>& pathPoints);
	};
}
