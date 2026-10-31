#pragma once
#include "Hydro/Terrain/Terrain.h"

#include <cstdint>

namespace Hydro
{
	class TerrainMesher;
	struct TerrainMeshBuilder;

	// Generated terrain decor (Terrain plan TR5): the instances of a terrain's decor rules (TerrainSettings::Decor),
	// placed from the seed and the terrain's data, so they are the same on every load and follow edits; nothing is
	// stored. They are part of the chunk's Parts mesh (TerrainPartKind::Decor, index rule * 256 + sprite): cut-out
	// quads and small boxes drawn with Standard materials, so they cast shadows and need no entities.
	class TerrainDecor
	{
	public:
		// Adds the chunk's decor to `parts`. A rule whose sprites aren't loaded (AssetManager::GetLoadedAsset) is skipped.
		static void AddChunk(const Terrain& terrain, const TerrainMesher& mesher, uint32_t chunkX, uint32_t chunkY, TerrainMeshBuilder& parts);
		// Whether every decor sprite of the terrain is loaded, so a chunk built now has all its decor.
		static bool AreSpritesLoaded(const Terrain& terrain);
	};
}
