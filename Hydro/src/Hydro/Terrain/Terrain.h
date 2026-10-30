#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Hydro
{
	// Terrain plan (wiki Terrain-Plan): a terraced terrain. Corners hold whole-step levels, cells hold ramp
	// directions and wall materials, a weight map holds the top layers. The reference for every rule is
	// scripts/terrain/terrain_mesher.py and splat.py (TR0).
	static constexpr uint32_t MaxTerrainLayers = 4;      // top layers: one RGBA8 weight map
	static constexpr uint32_t MaxTerrainWallLayers = 4;
	static constexpr uint32_t TerrainChunkCells = 32;    // a chunk is 32 x 32 cells (16 m at 0.5 m cells)
	static constexpr float TerrainArtPixelsPerMetre = 32.0f;
	static constexpr uint8_t TerrainWallHidden = 7;      // a cell's wall material: collision only, not drawn

	// A cell's ramp: which way it rises (TR3). Runs of cells with the same direction are one ramp.
	enum class TerrainRamp : uint8_t
	{
		None = 0,
		North,
		East,
		South,
		West
	};

	// One top layer: a material and how ragged its border with the others is.
	struct TerrainLayer
	{
		AssetHandle Material = 0;    // a MaterialAsset (.hymat): albedo, tint, tiling, filter; 0 = white
		float EdgeRoughness = 0.9f;  // edge noise in weight units (0 = clean border, 1 = very ragged)
		AssetHandle Fringe = 0;      // TR2: the cut-out strip hanging over walls below this layer (a .hymat); 0 = none
		float FringeLength = 7.0f;   // TR2: art pixels

		bool operator==(const TerrainLayer& other) const
		{
			return Material == other.Material && EdgeRoughness == other.EdgeRoughness && Fringe == other.Fringe && FringeLength == other.FringeLength;
		}
		bool operator!=(const TerrainLayer& other) const { return (*this == other) == false; }
	};

	// One wall layer (TR2): what the walls of the cells painted with it look like.
	struct TerrainWallLayer
	{
		AssetHandle Material = 0;
		// Scales the outline jitter and wall noise (0..1). An edge takes the calmer of its two cells' values,
		// so it stays a function of the edge and the data. Approved: stone 0.65, earth bank 0.57.
		float Variation = 1.0f;
		bool CapStones = false; // a coping row along the wall tops (stone walls)

		bool operator==(const TerrainWallLayer& other) const
		{
			return Material == other.Material && Variation == other.Variation && CapStones == other.CapStones;
		}
		bool operator!=(const TerrainWallLayer& other) const { return (*this == other) == false; }
	};

	// How a terrain is built and looks. Plain data, saved in its .hyterrain. Defaults are the approved TR1
	// look (TR0 preset "tr1"); lengths in metres unless they say art pixels.
	struct TerrainSettings
	{
		float CellSize = 0.5f;
		float StepHeight = 0.25f;
		int32_t BaseLevel = 0;       // the level at z = 0 (levels go below it for channels and ponds)
		uint32_t Seed = 1234;        // outline jitter, edge noise and decor; the same seed rebuilds the same terrain
		uint32_t SplatResolution = 8; // weight map texels per metre

		// Outline jitter (TR2): a share of a cell, from a smooth field plus a little per-edge noise.
		float Jitter = 0.38f;
		float JitterSmooth = 0.75f; // share of the jitter from the smooth field
		float JitterScale = 2.0f;   // metres between the smooth field's lattice points

		// Edge noise between top layers: clusters of this many art pixels, plus a share of per-pixel noise.
		float EdgeClusterSize = 2.5f;
		float EdgePixelNoise = 0.35f;

		std::vector<TerrainLayer> Layers;         // 1..MaxTerrainLayers
		std::vector<TerrainWallLayer> WallLayers; // 0..MaxTerrainWallLayers

		// Edges (TR2), art pixels.
		float LipWidth = 3.0f;
		float LipFaceHeight = 2.0f;
		float WallLean = 1.0f;  // art pixels per metre of wall height
		float WallNoise = 1.5f; // art pixels, outwards only

		// Coping stones (TR2).
		AssetHandle CapStoneMaterial = 0;
		float CapPitch = 0.28f;
		float CapJoint = 0.015f;
		float CapDepth = 0.2f;
		glm::vec2 CapHeight{ 0.06f, 0.08f };
		float CapSizeJitter = 0.04f;
		float CapPositionJitter = 0.015f;
		float CapYawJitter = 3.0f; // degrees
		float CapMissing = 0.08f;

		bool operator==(const TerrainSettings& other) const;
		bool operator!=(const TerrainSettings& other) const { return (*this == other) == false; }
	};

	// A rectangle of cells or texels, both corners included.
	struct TerrainRect
	{
		glm::uvec2 Min{ 0 };
		glm::uvec2 Max{ 0 };
	};

	// A terrain asset (.hyterrain, TerrainSerializer): the grid and the painted weights. Shared by every entity
	// whose TerrainComponent names its handle; loaded and kept by the AssetManager (GetAsset<Terrain>).
	//
	// Editable like a MaterialAsset: the editor's brushes change it in place (it is only data), mark what they
	// changed (MarkWeightsChanged, MarkCellsChanged) so the TerrainCache rebuilds only that, and the editor writes
	// the file when the scene is saved (IsModified).
	//
	// Layout: corner (x, y) is at (x * CellSize, y * CellSize) in the terrain's own space, row-major from the
	// south-west corner; cell (x, y) lies between corners (x, y) and (x + 1, y + 1). The weight map's texel
	// (x, y) covers [x, x + 1] / SplatResolution metres, rows from the south (y = 0) as well.
	class Terrain : public Asset
	{
	public:
		// A flat terrain of `cornersX` x `cornersY` corners (at least 2 x 2) at level 0, painted with layer 0.
		Terrain(const TerrainSettings& settings, uint32_t cornersX, uint32_t cornersY);

		Terrain(const Terrain&) = delete;
		Terrain& operator=(const Terrain&) = delete;

		// Reads a .hyterrain file, without sharing: every call makes a new asset. nullptr (logged) when the file
		// can't be read. The AssetManager's loader calls this.
		static Ref<Terrain> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Terrain; }
		AssetType GetType() const override { return GetStaticType(); }

		const TerrainSettings& GetSettings() const { return m_Settings; }
		// Every chunk's geometry and decor change; the weight map keeps its size (a new SplatResolution or
		// CellSize needs a new terrain).
		void SetSettings(const TerrainSettings& settings);

		uint32_t GetCornerCountX() const { return m_CornersX; }
		uint32_t GetCornerCountY() const { return m_CornersY; }
		uint32_t GetCellCountX() const { return m_CornersX - 1; }
		uint32_t GetCellCountY() const { return m_CornersY - 1; }
		// Metres covered by the cells.
		glm::vec2 GetSize() const;
		uint32_t GetChunkCountX() const { return (GetCellCountX() + TerrainChunkCells - 1) / TerrainChunkCells; }
		uint32_t GetChunkCountY() const { return (GetCellCountY() + TerrainChunkCells - 1) / TerrainChunkCells; }

		// Corner levels in whole steps: a corner is at z = (level - BaseLevel) * StepHeight. The caller marks what it
		// changed (MarkCellsChanged).
		int8_t GetLevel(uint32_t x, uint32_t y) const { return m_Levels[(size_t)y * m_CornersX + x]; }
		void SetLevel(uint32_t x, uint32_t y, int8_t level) { m_Levels[(size_t)y * m_CornersX + x] = level; }
		const std::vector<int8_t>& GetLevels() const { return m_Levels; }

		TerrainRamp GetRamp(uint32_t cellX, uint32_t cellY) const { return (TerrainRamp)m_Ramps[(size_t)cellY * GetCellCountX() + cellX]; }
		void SetRamp(uint32_t cellX, uint32_t cellY, TerrainRamp ramp) { m_Ramps[(size_t)cellY * GetCellCountX() + cellX] = (uint8_t)ramp; }
		const std::vector<uint8_t>& GetRamps() const { return m_Ramps; }

		// 0..MaxTerrainWallLayers - 1, or TerrainWallHidden.
		uint8_t GetWallMaterial(uint32_t cellX, uint32_t cellY) const { return m_WallMaterials[(size_t)cellY * GetCellCountX() + cellX]; }
		void SetWallMaterial(uint32_t cellX, uint32_t cellY, uint8_t material) { m_WallMaterials[(size_t)cellY * GetCellCountX() + cellX] = material; }
		const std::vector<uint8_t>& GetWallMaterials() const { return m_WallMaterials; }

		// The weight map: RGBA8, one channel per top layer (layer 0 = R). Painting keeps the channels summing to 255.
		uint32_t GetWeightMapWidth() const { return m_WeightsWidth; }
		uint32_t GetWeightMapHeight() const { return m_WeightsHeight; }
		const std::vector<uint8_t>& GetWeights() const { return m_Weights; }
		glm::u8vec4 GetWeight(uint32_t x, uint32_t y) const;
		void SetWeight(uint32_t x, uint32_t y, const glm::u8vec4& weight);

		// The caller changed these texels: the GPU copy is updated before the next draw, and the decor of the
		// chunks under them changes (TR5). Marks the terrain modified.
		void MarkWeightsChanged(const TerrainRect& texels);
		// The caller changed levels, ramps or wall materials in these cells: their chunks' geometry, collision and
		// decor are rebuilt. Marks the terrain modified.
		void MarkCellsChanged(const TerrainRect& cells);

		// Change tracking for the TerrainCache: revisions only grow. A chunk index is y * GetChunkCountX() + x.
		uint64_t GetGeometryRevision(uint32_t chunk) const { return m_GeometryRevisions[chunk]; }
		uint64_t GetDecorRevision(uint32_t chunk) const { return m_DecorRevisions[chunk]; }
		uint64_t GetCollisionRevision() const { return m_CollisionRevision; }
		// The texels changed since the last call (their union), for uploading; false when none did.
		bool TakeChangedWeights(TerrainRect& texels);

		// Changed since it was loaded or last saved (the editor saves modified terrains with the scene).
		bool IsModified() const { return m_Modified; }
		void SetModified(bool modified) { m_Modified = modified; }

		// The serializer builds a terrain from saved data; false (nothing changed) when the sizes don't fit.
		bool SetData(std::vector<int8_t> levels, std::vector<uint8_t> ramps, std::vector<uint8_t> wallMaterials,
			std::vector<uint8_t> weights, uint32_t weightsWidth, uint32_t weightsHeight);

		// The weight map's size for a terrain of `cellsX` x `cellsY` cells (at least 1 x 1 texel).
		static glm::uvec2 GetWeightMapSize(const TerrainSettings& settings, uint32_t cellsX, uint32_t cellsY);
		// Default settings for a new terrain: two layers (grass and path, say), no wall layers.
		static TerrainSettings GetDefaultSettings();

	private:
		void MarkChunks(const TerrainRect& cells, bool geometry, bool decor);

	private:
		TerrainSettings m_Settings;
		uint32_t m_CornersX = 2;
		uint32_t m_CornersY = 2;
		std::vector<int8_t> m_Levels;
		std::vector<uint8_t> m_Ramps;
		std::vector<uint8_t> m_WallMaterials;
		std::vector<uint8_t> m_Weights;
		uint32_t m_WeightsWidth = 1;
		uint32_t m_WeightsHeight = 1;

		uint64_t m_Revision = 1;
		std::vector<uint64_t> m_GeometryRevisions;
		std::vector<uint64_t> m_DecorRevisions;
		uint64_t m_CollisionRevision = 1;
		bool m_WeightsChanged = false;
		TerrainRect m_ChangedWeights;
		bool m_Modified = false;
	};
}
