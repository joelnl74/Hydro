#pragma once
#include "Hydro/Terrain/Terrain.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Hydro
{
	// What the mesher reads from a terrain's shape, as scripts/terrain/terrain_mesher.py does: ramp runs, wall segments
	// with their outline distances, where corners and crossings sit. Only GetTopLayerAt reads the painted weights.
	class TerrainAnalysis
	{
	public:
		// A corner or a crossing point on a cell's boundary. Key is global and canonical: the corner (i, j), or the
		// grid edge (i, j, H) from corner (i, j) to (i + 1, j) or (i, j, V) from (i, j) to (i, j + 1).
		struct BoundaryPoint
		{
			uint64_t Key = 0;
			uint8_t Slot = 0; // in the cell's 8-slot cycle, counter-clockwise from the south-west corner (even = corners)
			int32_t Lo = 0;
			int32_t Hi = 0;
		};

		// A wall's piece in one cell: the outline of Region(>= Level) from A to B (the region on the left).
		struct WallSegment
		{
			int32_t Level = 0;
			BoundaryPoint A;
			BoundaryPoint B;
			glm::uvec2 Cell{ 0 };
			uint8_t Material = 0;
			bool IsTop = false; // the highest wall at this outline: gets the lip, fringe and caps
			float U = 0.0f;     // distance along the outline at A (metres)
		};

		// A validated ramp cell: the heights (in levels, fractional) of its corners SW, SE, NE, NW.
		struct RampCell
		{
			bool Valid = false;
			float Heights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		};

		// Hash tags, as in scripts/terrain/terrain_mesher.py, so the different uses never correlate.
		static constexpr int64_t TagCross = 1;
		static constexpr int64_t TagWallNoise = 2;
		static constexpr int64_t TagCap = 3;
		static constexpr int64_t TagSplatCluster = 4;
		static constexpr int64_t TagSplatPixel = 5;

		static constexpr uint32_t EdgeH = 0; // corner (i, j) to (i + 1, j)
		static constexpr uint32_t EdgeV = 1; // corner (i, j) to (i, j + 1)

		// The kinds of BoundaryPoint::Key, and its parts.
		static constexpr uint64_t KeyCorner = 0;
		static constexpr uint64_t KeyEdgeH = 1;
		static constexpr uint64_t KeyEdgeV = 2;
		static uint64_t MakeKey(uint64_t kind, int64_t i, int64_t j)
		{
			return (kind << 60) | ((uint64_t)(i & 0x3FFFFFFF) << 30) | (uint64_t)(j & 0x3FFFFFFF);
		}
		static uint64_t KeyKind(uint64_t key) { return key >> 60; }
		static int64_t KeyI(uint64_t key) { return (int64_t)((key >> 30) & 0x3FFFFFFF); }
		static int64_t KeyJ(uint64_t key) { return (int64_t)(key & 0x3FFFFFFF); }
		static uint32_t KeyOrient(uint64_t key) { return KeyKind(key) == KeyEdgeH ? EdgeH : EdgeV; }

	public:
		explicit TerrainAnalysis(const Terrain& terrain);

		// What was found wrong (an invalid ramp run is drawn as a cliff), for the inspector.
		const std::vector<std::string>& GetProblems() const { return m_Problems; }
		// Whether the chunk has walls (painting there changes its fringe).
		bool ChunkHasWalls(uint32_t chunkX, uint32_t chunkY) const;

		// Every wall segment, cell by cell; the segments of cell (x, y); where a segment runs at a level (its top is at
		// segment.Level, its foot at segment.Level - 1).
		const std::vector<WallSegment>& GetWallSegments() const { return m_Segments; }
		void GetCellSegments(uint32_t cellX, uint32_t cellY, uint32_t& first, uint32_t& end) const;
		void GetSegmentLine(const WallSegment& segment, int32_t level, glm::vec3& a, glm::vec3& b) const;
		const RampCell& GetRamp(uint32_t i, uint32_t j) const { return m_RampCells[(size_t)j * m_CellsX + i]; }
		bool IsRamp(uint32_t cellX, uint32_t cellY) const { return GetRamp(cellX, cellY).Valid; }
		// The ground's height at a point (the band or ramp under it; the top of a wall's shelf on the wall line).
		float GetHeightAt(const glm::vec2& position) const;

		// A corner's level and a level's height (metres).
		int32_t Level(int64_t i, int64_t j) const;
		float LevelZ(float level) const;
		// Cell (i, j)'s boundary points, counter-clockwise from its south-west corner; returns how many (up to 8).
		uint32_t CellCycle(uint32_t i, uint32_t j, BoundaryPoint* points) const;
		// An edge's wall Slope, its crossing at `level` (jittered, leaned), and the direction down it (a wall's outward).
		float EdgeSlope(uint32_t orient, int64_t i, int64_t j) const;
		glm::vec3 EdgePoint(uint32_t orient, int64_t i, int64_t j, int32_t level) const;
		glm::vec2 EdgeDownDirection(uint32_t orient, int64_t i, int64_t j) const;
		// The same for the edge a boundary point lies on; a corner has no Slope.
		glm::vec2 EdgeDownDirection(const BoundaryPoint& point) const
		{
			return EdgeDownDirection(KeyOrient(point.Key), KeyI(point.Key), KeyJ(point.Key));
		}
		float EdgeSlope(const BoundaryPoint& point) const
		{
			return KeyKind(point.Key) == KeyCorner ? 0.0f : EdgeSlope(KeyOrient(point.Key), KeyI(point.Key), KeyJ(point.Key));
		}
		// A boundary point at `level`: on the outline, or stepped out by the wall's Slope (walls and their decor).
		glm::vec3 PointAt(const BoundaryPoint& point, int32_t level) const;
		glm::vec3 WallPointAt(const BoundaryPoint& point, int32_t level) const;

		// Region(>= level) in a cell: the cycle's points at that level or higher, in order.
		static uint32_t Region(const BoundaryPoint* cycle, uint32_t count, int32_t level, BoundaryPoint* out);
		// The visible tops of a cell: per level t, Region(>= t) - Region(>= t + 1), as convex caps (terrain_mesher.bands).
		template<typename Emit>
		static void ForEachBand(const BoundaryPoint* cycle, uint32_t count, Emit&& emit);

		// The top layer that wins at a point: terrain.frag's pixel splat on the CPU.
		static uint32_t GetTopLayerAt(const Terrain& terrain, const glm::vec2& position);
		// terrain_mesher.hash_u32 (negative values wrap as uint32), and the same as 0..1.
		static uint32_t Hash(std::initializer_list<int64_t> values);
		static float Hash01(std::initializer_list<int64_t> values);

	private:
		void FindRampRuns();
		void CollectWallSegments();
		void ComputeOutlineDistances();

		float EdgeVariation(uint32_t orient, int64_t i, int64_t j) const;
		float CrossingParam(uint32_t orient, int64_t i, int64_t j) const;

	private:
		const Terrain& m_Terrain;
		uint32_t m_CellsX = 0;
		uint32_t m_CellsY = 0;
		std::vector<RampCell> m_RampCells;
		std::vector<WallSegment> m_Segments;  // every wall segment, cell by cell (row-major)
		std::vector<uint32_t> m_FirstSegment; // per cell: its segments are [first[c], first[c + 1])
		std::vector<std::string> m_Problems;
	};

	template<typename Emit>
	void TerrainAnalysis::ForEachBand(const BoundaryPoint* cycle, uint32_t count, Emit&& emit)
	{
		int32_t lmin = cycle[0].Lo;
		int32_t lmax = cycle[0].Hi;
		for (uint32_t k = 0; k < count; k++)
		{
			lmin = std::min(lmin, cycle[k].Lo);
			lmax = std::max(lmax, cycle[k].Hi);
		}
		BoundaryPoint outer[8];
		BoundaryPoint inner[8];
		BoundaryPoint polygon[10];
		for (int32_t t = lmin; t <= lmax; t++)
		{
			const uint32_t outerCount = Region(cycle, count, t, outer);
			const uint32_t innerCount = Region(cycle, count, t + 1, inner);
			if (innerCount == 0)
			{
				emit(t, outer, outerCount);
				continue;
			}
			for (uint32_t k = 0; k < innerCount; k++)
			{
				const BoundaryPoint& a = inner[k];
				const BoundaryPoint& b = inner[(k + 1) % innerCount];
				// The cycle's points strictly after a and before b, counter-clockwise, that are in the outer region.
				uint32_t from = 0;
				while (cycle[from].Key != a.Key)
				{
					from++;
				}
				uint32_t n = 0;
				polygon[n++] = a;
				for (uint32_t c = (from + 1) % count; cycle[c].Key != b.Key; c = (c + 1) % count)
				{
					if (cycle[c].Hi >= t)
					{
						polygon[n++] = cycle[c];
					}
				}
				if (n > 1)
				{
					polygon[n++] = b;
					emit(t, polygon, n);
				}
			}
		}
	}
}
