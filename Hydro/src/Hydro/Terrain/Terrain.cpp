#include "hypch.h"
#include "Terrain.h"
#include "TerrainSerializer.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	bool TerrainSettings::operator==(const TerrainSettings& other) const
	{
		return CellSize == other.CellSize && StepHeight == other.StepHeight && BaseLevel == other.BaseLevel && Seed == other.Seed
			&& SplatResolution == other.SplatResolution && Jitter == other.Jitter && JitterSmooth == other.JitterSmooth
			&& JitterScale == other.JitterScale && EdgeClusterSize == other.EdgeClusterSize && EdgePixelNoise == other.EdgePixelNoise
			&& Layers == other.Layers && WallLayers == other.WallLayers && LipWidth == other.LipWidth && LipFaceHeight == other.LipFaceHeight
			&& WallLean == other.WallLean && WallNoise == other.WallNoise && CapStoneMaterial == other.CapStoneMaterial
			&& CapPitch == other.CapPitch && CapJoint == other.CapJoint && CapDepth == other.CapDepth && CapHeight == other.CapHeight
			&& CapSizeJitter == other.CapSizeJitter && CapPositionJitter == other.CapPositionJitter && CapYawJitter == other.CapYawJitter
			&& CapMissing == other.CapMissing;
	}

	Terrain::Terrain(const TerrainSettings& settings, uint32_t cornersX, uint32_t cornersY)
		: m_Settings(settings), m_CornersX(std::max(cornersX, 2u)), m_CornersY(std::max(cornersY, 2u))
	{
		if (m_Settings.Layers.empty())
		{
			m_Settings.Layers.push_back(TerrainLayer{});
		}

		const size_t corners = (size_t)m_CornersX * m_CornersY;
		const size_t cells = (size_t)GetCellCountX() * GetCellCountY();
		m_Levels.assign(corners, 0);
		m_Ramps.assign(cells, (uint8_t)TerrainRamp::None);
		m_WallMaterials.assign(cells, 0);

		const glm::uvec2 weights = GetWeightMapSize(m_Settings, GetCellCountX(), GetCellCountY());
		m_WeightsWidth = weights.x;
		m_WeightsHeight = weights.y;
		m_Weights.assign((size_t)m_WeightsWidth * m_WeightsHeight * 4, 0);
		for (size_t i = 0; i < m_Weights.size(); i += 4)
		{
			m_Weights[i] = 255; // all layer 0
		}

		const size_t chunks = (size_t)GetChunkCountX() * GetChunkCountY();
		m_GeometryRevisions.assign(chunks, m_Revision);
		m_DecorRevisions.assign(chunks, m_Revision);
	}

	Ref<Terrain> Terrain::LoadUncached(const std::filesystem::path& path)
	{
		return TerrainSerializer::Deserialize(path);
	}

	void Terrain::SetSettings(const TerrainSettings& settings)
	{
		if (settings == m_Settings)
		{
			return;
		}

		// The weight map's size follows CellSize and SplatResolution: keep the ones it was made with.
		m_Settings = settings;
		const glm::uvec2 weights = GetWeightMapSize(settings, GetCellCountX(), GetCellCountY());
		if (weights.x != m_WeightsWidth || weights.y != m_WeightsHeight)
		{
			HY_CORE_WARN("Terrain: CellSize and SplatResolution can't change on an existing terrain; kept as they were");
		}
		if (m_Settings.Layers.empty())
		{
			m_Settings.Layers.push_back(TerrainLayer{});
		}
		if (m_Settings.Layers.size() > MaxTerrainLayers)
		{
			m_Settings.Layers.resize(MaxTerrainLayers);
		}
		if (m_Settings.WallLayers.size() > MaxTerrainWallLayers)
		{
			m_Settings.WallLayers.resize(MaxTerrainWallLayers);
		}

		TerrainRect all;
		all.Max = glm::uvec2(GetCellCountX() - 1, GetCellCountY() - 1);
		MarkChunks(all, true, true);
		m_CollisionRevision = m_Revision;
		m_Modified = true;
	}

	glm::vec2 Terrain::GetSize() const
	{
		return glm::vec2((float)GetCellCountX(), (float)GetCellCountY()) * m_Settings.CellSize;
	}

	glm::u8vec4 Terrain::GetWeight(uint32_t x, uint32_t y) const
	{
		const size_t i = ((size_t)y * m_WeightsWidth + x) * 4;
		return glm::u8vec4(m_Weights[i], m_Weights[i + 1], m_Weights[i + 2], m_Weights[i + 3]);
	}

	void Terrain::SetWeight(uint32_t x, uint32_t y, const glm::u8vec4& weight)
	{
		const size_t i = ((size_t)y * m_WeightsWidth + x) * 4;
		m_Weights[i] = weight.x;
		m_Weights[i + 1] = weight.y;
		m_Weights[i + 2] = weight.z;
		m_Weights[i + 3] = weight.w;
	}

	void Terrain::MarkChunks(const TerrainRect& cells, bool geometry, bool decor)
	{
		m_Revision++;
		const uint32_t chunkMinX = std::min(cells.Min.x, GetCellCountX() - 1) / TerrainChunkCells;
		const uint32_t chunkMinY = std::min(cells.Min.y, GetCellCountY() - 1) / TerrainChunkCells;
		const uint32_t chunkMaxX = std::min(cells.Max.x, GetCellCountX() - 1) / TerrainChunkCells;
		const uint32_t chunkMaxY = std::min(cells.Max.y, GetCellCountY() - 1) / TerrainChunkCells;
		for (uint32_t y = chunkMinY; y <= chunkMaxY; y++)
		{
			for (uint32_t x = chunkMinX; x <= chunkMaxX; x++)
			{
				const uint32_t chunk = y * GetChunkCountX() + x;
				if (geometry)
				{
					m_GeometryRevisions[chunk] = m_Revision;
				}
				if (decor)
				{
					m_DecorRevisions[chunk] = m_Revision;
				}
			}
		}
	}

	void Terrain::MarkWeightsChanged(const TerrainRect& texels)
	{
		const TerrainRect clamped{ glm::min(texels.Min, glm::uvec2(m_WeightsWidth - 1, m_WeightsHeight - 1)),
			glm::min(texels.Max, glm::uvec2(m_WeightsWidth - 1, m_WeightsHeight - 1)) };
		if (m_WeightsChanged)
		{
			m_ChangedWeights.Min = glm::min(m_ChangedWeights.Min, clamped.Min);
			m_ChangedWeights.Max = glm::max(m_ChangedWeights.Max, clamped.Max);
		}
		else
		{
			m_ChangedWeights = clamped;
			m_WeightsChanged = true;
		}

		// The cells under the texels: their decor follows the layers (TR5). Their geometry does too where they
		// have walls (the fringe follows the winning layer, TR2); TR1 terrains have none.
		const float texelsPerCell = m_Settings.CellSize * (float)m_Settings.SplatResolution;
		TerrainRect cells;
		cells.Min = glm::uvec2(glm::vec2(clamped.Min) / texelsPerCell);
		cells.Max = glm::uvec2(glm::vec2(clamped.Max) / texelsPerCell);
		MarkChunks(cells, false, true);
		m_Modified = true;
	}

	void Terrain::MarkCellsChanged(const TerrainRect& cells)
	{
		MarkChunks(cells, true, true);
		m_CollisionRevision = m_Revision;
		m_Modified = true;
	}

	bool Terrain::TakeChangedWeights(TerrainRect& texels)
	{
		if (m_WeightsChanged == false)
		{
			return false;
		}
		texels = m_ChangedWeights;
		m_WeightsChanged = false;
		return true;
	}

	bool Terrain::SetData(std::vector<int8_t> levels, std::vector<uint8_t> ramps, std::vector<uint8_t> wallMaterials,
		std::vector<uint8_t> weights, uint32_t weightsWidth, uint32_t weightsHeight)
	{
		const size_t cells = (size_t)GetCellCountX() * GetCellCountY();
		if (levels.size() != m_Levels.size() || ramps.size() != cells || wallMaterials.size() != cells
			|| weights.size() != (size_t)weightsWidth * weightsHeight * 4 || weightsWidth == 0 || weightsHeight == 0)
		{
			return false;
		}

		m_Levels = std::move(levels);
		m_Ramps = std::move(ramps);
		m_WallMaterials = std::move(wallMaterials);
		m_Weights = std::move(weights);
		m_WeightsWidth = weightsWidth;
		m_WeightsHeight = weightsHeight;
		m_Revision++;
		std::fill(m_GeometryRevisions.begin(), m_GeometryRevisions.end(), m_Revision);
		std::fill(m_DecorRevisions.begin(), m_DecorRevisions.end(), m_Revision);
		m_CollisionRevision = m_Revision;
		m_WeightsChanged = false;
		return true;
	}

	glm::uvec2 Terrain::GetWeightMapSize(const TerrainSettings& settings, uint32_t cellsX, uint32_t cellsY)
	{
		const float perCell = settings.CellSize * (float)settings.SplatResolution;
		const uint32_t width = (uint32_t)std::lround((double)cellsX * perCell);
		const uint32_t height = (uint32_t)std::lround((double)cellsY * perCell);
		return glm::uvec2(std::max(width, 1u), std::max(height, 1u));
	}

	TerrainSettings Terrain::GetDefaultSettings()
	{
		TerrainSettings settings;
		settings.Layers.resize(2);
		return settings;
	}
}
