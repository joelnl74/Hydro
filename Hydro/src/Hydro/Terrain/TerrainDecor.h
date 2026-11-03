#pragma once
#include "Hydro/Terrain/Terrain.h"

#include <cstdint>

namespace Hydro
{
	class TerrainMesher;
	struct TerrainMeshBuilder;

	// Generated terrain decor: the instances of a terrain's decor rules, placed from the seed and the data (nothing
	// stored), as cut-out quads and small boxes in the chunk's Parts mesh, so they cast shadows and need no entities.
	class TerrainDecor
	{
	public:
		// A rule whose sprites aren't loaded is skipped.
		static void AddChunk(const Terrain& terrain, const TerrainMesher& mesher, uint32_t chunkX, uint32_t chunkY,
			TerrainMeshBuilder& parts);
		// So a chunk built now has all its decor.
		static bool AreSpritesLoaded(const Terrain& terrain);
	};
}
