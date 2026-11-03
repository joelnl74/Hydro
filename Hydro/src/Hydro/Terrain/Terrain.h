#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Hydro
{
	// A terraced terrain: corners hold whole-step levels, cells hold ramps and wall materials, a weight map
	// holds the top layers. The reference for its rules is scripts/terrain/terrain_mesher.py and splat.py.
	static constexpr uint32_t MaxTerrainLayers = 4;      // top layers: one RGBA8 weight map
	static constexpr uint32_t MaxTerrainWallLayers = 4;
	static constexpr uint32_t TerrainChunkCells = 32;    // a chunk is 32 x 32 cells (16 m at 0.5 m cells)
	static constexpr float TerrainArtPixelsPerMetre = 32.0f;
	static constexpr uint8_t TerrainWallHidden = 7;      // a cell's wall material: collision only, not drawn
	static constexpr uint32_t MaxTerrainDecorRules = 8;
	static constexpr uint32_t MaxTerrainDecorSprites = 64; // per rule
	static constexpr uint32_t TerrainDecorAnyLayer = 255;  // a wall decor rule for every wall layer

	// A cell's ramp: which way it rises. Runs of cells with the same direction are one ramp.
	enum class TerrainRamp : uint8_t
	{
		None = 0,
		North,
		East,
		South,
		West
	};

	// One top layer: a material and how its border with the others looks.
	struct TerrainLayer
	{
		AssetHandle Material = 0;    // a MaterialAsset (.hymat): albedo, tint, tiling, filter; 0 = white
		float EdgeRoughness = 0.9f;  // edge noise in weight units (0 = clean border, 1 = very ragged)
		// Width, in weight units, of the band mixed with the runner-up layer (75 % and 50 % steps, so it stays
		// pixel art); 0 = hard pixel borders.
		float Blend = 0.5f;
		// How much the layer's texture brightness raises its score at the border (bright blades stand out, dark
		// gaps let the neighbour through); 0 = off.
		float HeightBlend = 1.0f;
		AssetHandle Fringe = 0;      // cut-out strip hanging over the walls below this layer (a .hymat); 0 = none
		float FringeLength = 7.0f;   // art pixels

		bool operator==(const TerrainLayer& other) const
		{
			return Material == other.Material && EdgeRoughness == other.EdgeRoughness && Blend == other.Blend
				&& HeightBlend == other.HeightBlend
				&& Fringe == other.Fringe && FringeLength == other.FringeLength;
		}
		bool operator!=(const TerrainLayer& other) const { return (*this == other) == false; }
	};

	// One wall layer: what the walls of the cells painted with it look like.
	struct TerrainWallLayer
	{
		AssetHandle Material = 0;
		// Scales the outline jitter and wall noise (0..1). An edge takes the calmer of its two cells' values.
		float Variation = 1.0f;
		bool CapStones = false; // a coping row along the wall tops (stone walls)
		// How far the wall's foot steps out per metre of height (0 = upright; earth banks 0.4 to 0.55). An edge
		// takes the smaller of its two cells' values, so neighbouring walls meet without a crack.
		float Slope = 0.0f;

		bool operator==(const TerrainWallLayer& other) const
		{
			return Material == other.Material && Variation == other.Variation && CapStones == other.CapStones && Slope == other.Slope;
		}
		bool operator!=(const TerrainWallLayer& other) const { return (*this == other) == false; }
	};

	// Where a decor rule places its instances.
	enum class TerrainDecorPlace : uint8_t
	{
		Layer = 0,   // scattered where the top layer wins
		LayerBorder, // where the top layer meets another one
		WallTop,     // along the top of walls (behind the lip), per metre of wall
		WallFoot     // along the foot of walls, on the lower ground, per metre of wall
	};

	enum class TerrainDecorShape : uint8_t
	{
		Upright = 0, // a sprite standing up, facing south (the field cameras look north): tufts, flowers, ferns
		Flat,        // a sprite lying on the ground, turned at random: petals, leaf litter
		Stone        // a small box textured with the sprite's image: pebbles
	};

	// Small repeated dressing generated from the terrain: no entities, the same on every load, follows edits.
	struct TerrainDecorRule
	{
		TerrainDecorPlace Place = TerrainDecorPlace::Layer;
		uint32_t Layer = 0; // top layer (Layer, LayerBorder) or wall layer (WallTop, WallFoot; TerrainDecorAnyLayer = all)
		TerrainDecorShape Shape = TerrainDecorShape::Upright;
		std::vector<AssetHandle> Sprites; // textures (cut out at alpha 0.5); each instance picks one
		float Density = 1.0f;             // per square metre (Layer, LayerBorder) or per metre of wall
		glm::vec2 Scale{ 0.85f, 1.1f };   // of the sprite's own size (32 pixels per metre), min..max

		bool operator==(const TerrainDecorRule& other) const
		{
			return Place == other.Place && Layer == other.Layer && Shape == other.Shape && Sprites == other.Sprites
				&& Density == other.Density
				&& Scale == other.Scale;
		}
		bool operator!=(const TerrainDecorRule& other) const { return (*this == other) == false; }
	};

	// How a terrain is built and looks; saved in its .hyterrain. Lengths in metres unless they say art pixels.
	struct TerrainSettings
	{
		float CellSize = 0.5f;
		float StepHeight = 0.25f;
		int32_t BaseLevel = 0;       // the level at z = 0 (levels go below it for channels and ponds)
		uint32_t Seed = 1234;        // outline jitter, edge noise and decor; the same seed rebuilds the same terrain
		uint32_t SplatResolution = 8; // weight map texels per metre

		// Outline jitter: a share of a cell, from a smooth field plus a little per-edge noise.
		float Jitter = 0.38f;
		float JitterSmooth = 0.75f; // share of the jitter from the smooth field
		float JitterScale = 2.0f;   // metres between the smooth field's lattice points

		// Edge noise between top layers: clusters of this many art pixels, plus a share of per-pixel noise.
		float EdgeClusterSize = 2.5f;
		float EdgePixelNoise = 0.35f;

		std::vector<TerrainLayer> Layers;         // 1..MaxTerrainLayers
		std::vector<TerrainWallLayer> WallLayers; // 0..MaxTerrainWallLayers

		// Edges, in art pixels.
		float LipWidth = 3.0f;
		float LipFaceHeight = 2.0f;
		float WallLean = 1.0f;  // art pixels per metre of wall height
		float WallNoise = 1.5f; // art pixels, outwards only
		// Walls are lit with their normal turned this far (0..1) towards straight up, so faces turned from the sun
		// aren't near-black. Drawing only: collision is untouched.
		float WallLight = 0.35f;

		// Coping stones.
		AssetHandle CapStoneMaterial = 0;
		float CapPitch = 0.28f;
		float CapJoint = 0.015f;
		float CapDepth = 0.2f;
		glm::vec2 CapHeight{ 0.06f, 0.08f };
		float CapSizeJitter = 0.04f;
		float CapPositionJitter = 0.015f;
		float CapYawJitter = 3.0f; // degrees
		float CapMissing = 0.08f;

		std::vector<TerrainDecorRule> Decor; // 0..MaxTerrainDecorRules

		bool operator==(const TerrainSettings& other) const;
		bool operator!=(const TerrainSettings& other) const { return (*this == other) == false; }
	};

	// A terrain's collision triangles in its own space, welded so shared edges are shared.
	struct TerrainCollisionMesh
	{
		std::vector<glm::vec3> Positions;
		std::vector<uint32_t> Indices;
	};

	// A rectangle of cells or texels, both corners included.
	struct TerrainRect
	{
		glm::uvec2 Min{ 0 };
		glm::uvec2 Max{ 0 };
	};

	// A terrain asset (.hyterrain): the grid and the painted weights, shared by every TerrainComponent naming it.
	// Corner (x, y) is at (x, y) * CellSize, rows from the south-west; see docs/Architecture.md, Terrains.
	class Terrain : public Asset
	{
	public:
		// A flat terrain of `cornersX` x `cornersY` corners (at least 2 x 2) at level 0, painted with layer 0.
		Terrain(const TerrainSettings& settings, uint32_t cornersX, uint32_t cornersY);

		Terrain(const Terrain&) = delete;
		Terrain& operator=(const Terrain&) = delete;

		// Reads a .hyterrain file into a new asset (no sharing); nullptr (logged) when it can't be read.
		static Ref<Terrain> LoadUncached(const std::filesystem::path& path);

		static AssetType GetStaticType() { return AssetType::Terrain; }
		AssetType GetType() const override { return GetStaticType(); }

		const TerrainSettings& GetSettings() const { return m_Settings; }
		// Changes every chunk's geometry and decor; the weight map keeps its size.
		void SetSettings(const TerrainSettings& settings);

		uint32_t GetCornerCountX() const { return m_CornersX; }
		uint32_t GetCornerCountY() const { return m_CornersY; }
		uint32_t GetCellCountX() const { return m_CornersX - 1; }
		uint32_t GetCellCountY() const { return m_CornersY - 1; }
		glm::vec2 GetSize() const;
		uint32_t GetChunkCountX() const { return (GetCellCountX() + TerrainChunkCells - 1) / TerrainChunkCells; }
		uint32_t GetChunkCountY() const { return (GetCellCountY() + TerrainChunkCells - 1) / TerrainChunkCells; }

		// Corner levels in whole steps: a corner is at z = (level - BaseLevel) * StepHeight.
		int8_t GetLevel(uint32_t x, uint32_t y) const { return m_Levels[(size_t)y * m_CornersX + x]; }
		void SetLevel(uint32_t x, uint32_t y, int8_t level) { m_Levels[(size_t)y * m_CornersX + x] = level; }
		const std::vector<int8_t>& GetLevels() const { return m_Levels; }

		TerrainRamp GetRamp(uint32_t cellX, uint32_t cellY) const { return (TerrainRamp)m_Ramps[(size_t)cellY * GetCellCountX() + cellX]; }
		void SetRamp(uint32_t cellX, uint32_t cellY, TerrainRamp ramp) { m_Ramps[(size_t)cellY * GetCellCountX() + cellX] = (uint8_t)ramp; }
		const std::vector<uint8_t>& GetRamps() const { return m_Ramps; }

		// 0..MaxTerrainWallLayers - 1, or TerrainWallHidden.
		uint8_t GetWallMaterial(uint32_t cellX, uint32_t cellY) const
		{
			return m_WallMaterials[(size_t)cellY * GetCellCountX() + cellX];
		}
		void SetWallMaterial(uint32_t cellX, uint32_t cellY, uint8_t material)
		{
			m_WallMaterials[(size_t)cellY * GetCellCountX() + cellX] = material;
		}
		const std::vector<uint8_t>& GetWallMaterials() const { return m_WallMaterials; }

		// The weight map: RGBA8, one channel per top layer (layer 0 = R). Painting keeps the channels summing to 255.
		uint32_t GetWeightMapWidth() const { return m_WeightsWidth; }
		uint32_t GetWeightMapHeight() const { return m_WeightsHeight; }
		const std::vector<uint8_t>& GetWeights() const { return m_Weights; }
		glm::u8vec4 GetWeight(uint32_t x, uint32_t y) const;
		void SetWeight(uint32_t x, uint32_t y, const glm::u8vec4& weight);

		// The caller changed these texels: they are uploaded before the next draw and their chunks' decor changes.
		void MarkWeightsChanged(const TerrainRect& texels);
		// The caller changed levels, ramps or wall materials in these cells: their chunks are rebuilt.
		void MarkCellsChanged(const TerrainRect& cells);

		// Change tracking for the TerrainCache: revisions only grow. A chunk index is y * GetChunkCountX() + x.
		uint64_t GetGeometryRevision(uint32_t chunk) const { return m_GeometryRevisions[chunk]; }
		uint64_t GetDecorRevision(uint32_t chunk) const { return m_DecorRevisions[chunk]; }
		uint64_t GetCollisionRevision() const { return m_CollisionRevision; }
		// The texels changed since the last call (their union), for uploading; false when none did.
		bool TakeChangedWeights(TerrainRect& texels);

		// Tops, ramps and walls (Hidden ones too), rebuilt on first use after the shape changed.
		const TerrainCollisionMesh& GetCollisionMesh() const;

		// Changed since it was loaded or last saved.
		bool IsModified() const { return m_Modified; }
		void SetModified(bool modified) { m_Modified = modified; }

		// Builds the terrain from saved data; false (nothing changed) when the sizes don't fit.
		bool SetData(std::vector<int8_t> levels, std::vector<uint8_t> ramps, std::vector<uint8_t> wallMaterials,
			std::vector<uint8_t> weights, uint32_t weightsWidth, uint32_t weightsHeight);

		// At least 1 x 1 texel.
		static glm::uvec2 GetWeightMapSize(const TerrainSettings& settings, uint32_t cellsX, uint32_t cellsY);
		// Two layers, no wall layers.
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

		mutable TerrainCollisionMesh m_Collision; // derived data, cached
		mutable uint64_t m_CollisionBuilt = 0;    // the collision revision it was built from
	};
}
