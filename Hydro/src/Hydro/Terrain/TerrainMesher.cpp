#include "hypch.h"
#include "TerrainMesher.h"

#include <algorithm>

namespace Hydro
{
	MeshData TerrainMesher::BuildChunkTops(const Terrain& terrain, uint32_t chunkX, uint32_t chunkY, bool& hasSteps)
	{
		hasSteps = false;
		const TerrainSettings& settings = terrain.GetSettings();
		const uint32_t cellMinX = chunkX * TerrainChunkCells;
		const uint32_t cellMinY = chunkY * TerrainChunkCells;
		const uint32_t cellMaxX = std::min(cellMinX + TerrainChunkCells, terrain.GetCellCountX());
		const uint32_t cellMaxY = std::min(cellMinY + TerrainChunkCells, terrain.GetCellCountY());

		MeshData data;
		const size_t cells = (size_t)(cellMaxX - cellMinX) * (cellMaxY - cellMinY);
		data.Vertices.reserve(cells * 4);
		data.Indices.reserve(cells * 6);

		for (uint32_t y = cellMinY; y < cellMaxY; y++)
		{
			for (uint32_t x = cellMinX; x < cellMaxX; x++)
			{
				const int8_t a = terrain.GetLevel(x, y);
				const int8_t b = terrain.GetLevel(x + 1, y);
				const int8_t c = terrain.GetLevel(x + 1, y + 1);
				const int8_t d = terrain.GetLevel(x, y + 1);
				const int8_t lowest = std::min(std::min(a, b), std::min(c, d));
				if (a != b || b != c || c != d)
				{
					hasSteps = true;
				}

				const float z = (float)(lowest - settings.BaseLevel) * settings.StepHeight;
				const glm::vec2 corners[4] = {
					glm::vec2((float)x, (float)y) * settings.CellSize,
					glm::vec2((float)(x + 1), (float)y) * settings.CellSize,
					glm::vec2((float)(x + 1), (float)(y + 1)) * settings.CellSize,
					glm::vec2((float)x, (float)(y + 1)) * settings.CellSize
				};

				const uint32_t first = (uint32_t)data.Vertices.size();
				for (const glm::vec2& corner : corners)
				{
					MeshVertex vertex;
					vertex.Position = glm::vec3(corner, z);
					vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
					vertex.UV = corner;
					data.Vertices.push_back(vertex);
				}
				// Counter-clockwise seen from above.
				const uint32_t quad[6] = { first, first + 1, first + 2, first, first + 2, first + 3 };
				data.Indices.insert(data.Indices.end(), quad, quad + 6);
			}
		}

		SubMesh subMesh;
		subMesh.FirstIndex = 0;
		subMesh.IndexCount = (uint32_t)data.Indices.size();
		subMesh.SourceName = "Terrain Tops";
		data.SubMeshes.push_back(subMesh);
		return data;
	}
}
