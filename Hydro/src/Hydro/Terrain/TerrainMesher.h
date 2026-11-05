#pragma once
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Terrain/Terrain.h"
#include "Hydro/Terrain/TerrainAnalysis.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	struct TerrainMeshBuilder;      // TerrainMeshBuilder.h
	struct TerrainCollisionBuilder; // TerrainMeshBuilder.h

	// The sub-meshes of a chunk's Parts mesh, by SubMesh::MaterialSlot (GetPartKind). Walls, lip faces and fringes
	// have one slot per layer.
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
		// Bands, lips and ramps, drawn with the terrain surface shader. UV = the position (x, y).
		MeshData Tops;
		// Walls, lip faces, fringes, cap stones and decor (Standard materials). UVs in metres with v = -z; fringes and
		// decor sprites 0..1.
		MeshData Parts;
	};

	// Turns a terrain's data into chunk meshes and collision from its TerrainAnalysis. Built once per shape revision:
	// the analysis ignores the painted weights.
	class TerrainMesher
	{
	public:
		explicit TerrainMesher(const Terrain& terrain);

		// The chunk's tops and parts. The fringe and decor follow the painted weights as they are now.
		TerrainChunkMesh BuildChunk(uint32_t chunkX, uint32_t chunkY) const;
		// Tops, ramps and walls (Hidden ones too); not the lip, fringe, caps or decor.
		void BuildCollision(std::vector<glm::vec3>& positions, std::vector<uint32_t>& indices) const;

		// What the meshes are built from: problems for the inspector, wall segments and heights for the decor.
		const TerrainAnalysis& GetAnalysis() const { return m_Analysis; }

		// A Parts sub-mesh's slot: its kind and an index (the wall or top layer; for decor, rule * 256 + sprite).
		static uint32_t GetPartSlot(TerrainPartKind kind, uint32_t index)
		{
			return ((uint32_t)kind << 16) | (index & 0xFFFF);
		}
		static TerrainPartKind GetPartKind(uint32_t slot) { return (TerrainPartKind)(slot >> 16); }
		static uint32_t GetPartIndex(uint32_t slot) { return slot & 0xFFFF; }

	private:
		using BoundaryPoint = TerrainAnalysis::BoundaryPoint;
		using WallSegment = TerrainAnalysis::WallSegment;

		// An edge's heights along it (0..1) between its two corners, through its crossings: what a ramp's side meets.
		void EdgeProfile(uint32_t orient, int64_t i, int64_t j, std::vector<glm::vec2>& profile) const;

		void AddTops(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainCollisionBuilder* collision) const;
		void AddRamp(uint32_t i, uint32_t j, TerrainMeshBuilder* tops, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const;
		void AddWall(const WallSegment& segment, TerrainMeshBuilder* parts, TerrainCollisionBuilder* collision) const;
		void AddWallEdges(const WallSegment& segment, TerrainMeshBuilder& tops, TerrainMeshBuilder& parts) const;
		glm::vec3 WallShading(const glm::vec3& at, const glm::vec3& bt, const glm::vec3& ab, const glm::vec3& bb) const;
		void AddCapStones(const WallSegment& segment, const glm::vec3& at, const glm::vec3& bt, TerrainMeshBuilder& parts) const;

	private:
		const Terrain& m_Terrain;
		TerrainAnalysis m_Analysis;
		uint32_t m_CellsX = 0;
		uint32_t m_CellsY = 0;
	};
}
