#pragma once
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Terrain/Terrain.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	struct TerrainMeshBuilder;      // TerrainMesher.cpp
	struct TerrainCollisionBuilder; // TerrainMesher.cpp

	// The sub-meshes of a chunk's Parts mesh: SubMesh::MaterialSlot says what each one is, so the renderer can pick
	// its material (TerrainMesher::GetPartKind). Walls, lip faces and fringes have one slot per layer.
	enum class TerrainPartKind : uint32_t
	{
		Wall = 0,  // a wall layer's material
		LipFace,   // a wall layer's material, 20 % darker
		Fringe,    // a top layer's Fringe material (cut out)
		CapStones, // TerrainSettings::CapStoneMaterial
		Decor      // a decor rule's material (TerrainDecor)
	};

	// One chunk's meshes, in the terrain's own space (Z up, metres, corner (0, 0) at the origin).
	struct TerrainChunkMesh
	{
		// The tops (bands, lips, ramps): one sub-mesh, drawn with the terrain surface shader. UV = the position (x, y).
		MeshData Tops;
		// Everything drawn with Standard materials: walls, lip faces, fringes, cap stones, decor. UVs in metres
		// (the material's Tiling is per metre; v = -z, as images have v = 0 at the top), fringes and decor sprites 0..1
		// (v = 0 at the top).
		MeshData Parts;
	};

	// Turns a terrain's data into meshes (Terrain plan, "Shape"; scripts/terrain/terrain_mesher.py is the reference:
	// same data, same hashes, same rules).
	//
	// Making one analyses the whole terrain once: the ramp runs and every wall segment with its distance along the
	// outline (so wall textures and cap stones run on across cells and chunks). Then BuildChunk meshes any chunk and
	// BuildCollision the whole terrain, both from that analysis. The analysis depends on the levels, ramps, wall
	// materials and settings only (not the painted weights): the TerrainCache keeps one per terrain and makes a new
	// one when Terrain::GetCollisionRevision changes.
	//
	//   TerrainMesher mesher(*terrain);
	//   TerrainChunkMesh chunk = mesher.BuildChunk(x, y);
	class TerrainMesher
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

	public:
		explicit TerrainMesher(const Terrain& terrain);

		// The chunk's tops and parts. The fringe and decor follow the painted weights as they are now.
		TerrainChunkMesh BuildChunk(uint32_t chunkX, uint32_t chunkY) const;
		// The whole terrain's collision (Terrain plan, "Component, rendering and collision"): tops, ramps and walls
		// (Hidden ones too); not the lip, fringe, caps or decor.
		void BuildCollision(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) const;

		// What the analysis found wrong (an invalid ramp run is drawn as a cliff), for the inspector.
		const std::vector<std::string>& GetProblems() const { return m_Problems; }
		// Whether the chunk has walls (painting there changes its fringe).
		bool ChunkHasWalls(uint32_t chunkX, uint32_t chunkY) const;

		// For the decor (TerrainDecor): every wall segment, cell by cell; the segments of cell (x, y); where a segment
		// runs at a level (its top is at segment.Level, its foot at segment.Level - 1); whether a cell is a valid ramp.
		const std::vector<WallSegment>& GetWallSegments() const;
		void GetCellSegments(uint32_t cellX, uint32_t cellY, uint32_t& first, uint32_t& end) const;
		void GetSegmentLine(const WallSegment& segment, int32_t level, glm::vec3& a, glm::vec3& b) const;
		bool IsRamp(uint32_t cellX, uint32_t cellY) const { return GetRamp(cellX, cellY).Valid; }
		// The ground's height at a point (the band or ramp under it; the top of a wall's shelf on the wall line).
		float GetHeightAt(const glm::vec2& position) const;

		// A Parts sub-mesh's slot: its kind and an index (the wall or top layer; for decor, rule * 256 + sprite).
		static uint32_t GetPartSlot(TerrainPartKind kind, uint32_t index) { return ((uint32_t)kind << 16) | (index & 0xFFFF); }
		static TerrainPartKind GetPartKind(uint32_t slot) { return (TerrainPartKind)(slot >> 16); }
		static uint32_t GetPartIndex(uint32_t slot) { return slot & 0xFFFF; }

		// The top layer that wins at a point of the terrain's space: the pixel splat of terrain.frag on the CPU.
		static uint32_t GetTopLayerAt(const Terrain& terrain, const glm::vec2& position);
		// terrain_mesher.hash_u32 (negative values wrap as uint32), and the same as 0..1.
		static uint32_t Hash(std::initializer_list<int64_t> values);
		static float Hash01(std::initializer_list<int64_t> values);

	private:
		void FindRampRuns();
		void CollectWallSegments();
		void ComputeOutlineDistances();

		int32_t Level(int64_t i, int64_t j) const;
		float LevelZ(float level) const;
		uint32_t CellCycle(uint32_t i, uint32_t j, BoundaryPoint* points) const;
		float EdgeVariation(uint32_t orient, int64_t i, int64_t j) const;
		float EdgeSlope(uint32_t orient, int64_t i, int64_t j) const;
		float CrossingParam(uint32_t orient, int64_t i, int64_t j) const;
		glm::vec3 EdgePoint(uint32_t orient, int64_t i, int64_t j, int32_t level) const;
		glm::vec2 EdgeDownDirection(uint32_t orient, int64_t i, int64_t j) const;
		glm::vec3 PointAt(const BoundaryPoint& point, int32_t level) const;
		glm::vec3 WallPointAt(const BoundaryPoint& point, int32_t level) const;
		const RampCell& GetRamp(uint32_t i, uint32_t j) const { return m_RampCells[(size_t)j * m_CellsX + i]; }

		void EdgeProfile(uint32_t orient, int64_t i, int64_t j, std::vector<glm::vec2>& profile) const;

		void AddTops(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainCollisionBuilder* collision) const;
		void AddRamp(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const;
		void AddWall(const WallSegment& segment, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const;
		void AddWallEdges(const WallSegment& segment, TerrainMeshBuilder& tops, TerrainMeshBuilder& parts) const;
		glm::vec3 WallShading(const glm::vec3& at, const glm::vec3& bt, const glm::vec3& ab, const glm::vec3& bb) const;
		void AddCapStones(const WallSegment& segment, const glm::vec3& at, const glm::vec3& bt, TerrainMeshBuilder& parts) const;

	private:
		const Terrain& m_Terrain;
		uint32_t m_CellsX = 0;
		uint32_t m_CellsY = 0;
		std::vector<RampCell> m_RampCells;
		std::vector<WallSegment> m_Segments;          // every wall segment, cell by cell (row-major)
		std::vector<uint32_t> m_FirstSegment;         // per cell: its segments are [first[c], first[c + 1])
		std::vector<std::string> m_Problems;
	};
}
