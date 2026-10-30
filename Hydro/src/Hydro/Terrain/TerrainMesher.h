#pragma once
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Terrain/Terrain.h"

#include <cstdint>

namespace Hydro
{
	// Turns a terrain's data into meshes, one chunk (TerrainChunkCells x TerrainChunkCells cells) at a time, in
	// the terrain's own space (Z up, metres, corner (0, 0) at the origin). Pure CPU: the TerrainCache uploads the
	// result and keeps its triangles for picking.
	//
	// TR1: the tops of flat cells, one quad per cell at its level. A cell whose corners differ is drawn flat at its
	// lowest corner until TR2 meshes terraces (regions, bands, walls, lip, fringe, caps: scripts/terrain/terrain_mesher.py
	// is the reference).
	//
	// Vertices: UV = the position in the terrain's space (x, y metres), which the terrain surface shader (terrain.frag)
	// reads to pick its layer per art pixel; normal up; colour white.
	class TerrainMesher
	{
	public:
		// The chunk's tops as one sub-mesh. `hasSteps` is set when a cell's corners differ (drawn flat for now).
		static MeshData BuildChunkTops(const Terrain& terrain, uint32_t chunkX, uint32_t chunkY, bool& hasSteps);
	};
}
