#include "TerrainStrokeCommand.h"

#include <Hydro/Asset/AssetManager.h>

#include <algorithm>

namespace Hydro
{
	TerrainSnapshot TerrainSnapshot::Take(const Terrain& terrain)
	{
		TerrainSnapshot snapshot;
		snapshot.Levels = terrain.GetLevels();
		snapshot.Ramps = terrain.GetRamps();
		snapshot.WallMaterials = terrain.GetWallMaterials();
		snapshot.Weights = terrain.GetWeights();
		return snapshot;
	}

	// The rectangle (in elements of `channels` values) where `a` and `b` differ; false when they don't.
	template<typename T>
	static bool FindChanges(const std::vector<T>& a, const std::vector<T>& b, uint32_t width, uint32_t channels, TerrainRect& rect)
	{
		if (a.size() != b.size() || width == 0)
		{
			return false;
		}
		bool any = false;
		const size_t count = a.size() / channels;
		for (size_t i = 0; i < count; i++)
		{
			bool differs = false;
			for (uint32_t c = 0; c < channels; c++)
			{
				differs = differs || a[i * channels + c] != b[i * channels + c];
			}
			if (differs == false)
			{
				continue;
			}
			const glm::uvec2 at((uint32_t)(i % width), (uint32_t)(i / width));
			if (any)
			{
				rect.Min = glm::min(rect.Min, at);
				rect.Max = glm::max(rect.Max, at);
			}
			else
			{
				rect.Min = rect.Max = at;
				any = true;
			}
		}
		return any;
	}

	template<typename T>
	std::vector<T> TerrainStrokeCommand::Cut(const std::vector<T>& values, uint32_t width, const TerrainRect& rect, uint32_t channels)
	{
		std::vector<T> out;
		out.reserve((size_t)(rect.Max.x - rect.Min.x + 1) * (rect.Max.y - rect.Min.y + 1) * channels);
		for (uint32_t y = rect.Min.y; y <= rect.Max.y; y++)
		{
			const size_t from = ((size_t)y * width + rect.Min.x) * channels;
			const size_t to = ((size_t)y * width + rect.Max.x + 1) * channels;
			out.insert(out.end(), values.begin() + from, values.begin() + to);
		}
		return out;
	}

	TerrainStrokeCommand::TerrainStrokeCommand(AssetHandle terrain, std::string name, const TerrainSnapshot& before, const Terrain& after)
		: m_Terrain(terrain), m_Name(std::move(name))
	{
		const uint32_t cornersX = after.GetCornerCountX();
		const uint32_t cellsX = after.GetCellCountX();
		const uint32_t texelsX = after.GetWeightMapWidth();

		m_HasCorners = FindChanges(before.Levels, after.GetLevels(), cornersX, 1, m_Corners);
		if (m_HasCorners)
		{
			m_LevelsBefore = Cut(before.Levels, cornersX, m_Corners, 1);
			m_LevelsAfter = Cut(after.GetLevels(), cornersX, m_Corners, 1);
		}

		TerrainRect ramps;
		TerrainRect walls;
		const bool rampsChanged = FindChanges(before.Ramps, after.GetRamps(), cellsX, 1, ramps);
		const bool wallsChanged = FindChanges(before.WallMaterials, after.GetWallMaterials(), cellsX, 1, walls);
		if (rampsChanged || wallsChanged)
		{
			m_HasCells = true;
			m_Cells = rampsChanged ? ramps : walls;
			if (rampsChanged && wallsChanged)
			{
				m_Cells.Min = glm::min(ramps.Min, walls.Min);
				m_Cells.Max = glm::max(ramps.Max, walls.Max);
			}
			m_RampsBefore = Cut(before.Ramps, cellsX, m_Cells, 1);
			m_RampsAfter = Cut(after.GetRamps(), cellsX, m_Cells, 1);
			m_WallsBefore = Cut(before.WallMaterials, cellsX, m_Cells, 1);
			m_WallsAfter = Cut(after.GetWallMaterials(), cellsX, m_Cells, 1);
		}

		m_HasTexels = FindChanges(before.Weights, after.GetWeights(), texelsX, 4, m_Texels);
		if (m_HasTexels)
		{
			m_WeightsBefore = Cut(before.Weights, texelsX, m_Texels, 4);
			m_WeightsAfter = Cut(after.GetWeights(), texelsX, m_Texels, 4);
		}
	}

	void TerrainStrokeCommand::Undo()
	{
		Apply(false);
	}

	void TerrainStrokeCommand::Redo()
	{
		Apply(true);
	}

	void TerrainStrokeCommand::Apply(bool after) const
	{
		const Ref<Terrain> terrain = AssetManager::GetLoadedAsset<Terrain>(m_Terrain);
		if (terrain == nullptr)
		{
			return;
		}
		// The terrain was replaced by one of another size: nothing to apply to.
		if ((m_HasCorners && (m_Corners.Max.x >= terrain->GetCornerCountX() || m_Corners.Max.y >= terrain->GetCornerCountY()))
			|| (m_HasCells && (m_Cells.Max.x >= terrain->GetCellCountX() || m_Cells.Max.y >= terrain->GetCellCountY()))
			|| (m_HasTexels && (m_Texels.Max.x >= terrain->GetWeightMapWidth() || m_Texels.Max.y >= terrain->GetWeightMapHeight())))
		{
			return;
		}

		if (m_HasCorners)
		{
			const std::vector<int8_t>& levels = after ? m_LevelsAfter : m_LevelsBefore;
			size_t i = 0;
			for (uint32_t y = m_Corners.Min.y; y <= m_Corners.Max.y; y++)
			{
				for (uint32_t x = m_Corners.Min.x; x <= m_Corners.Max.x; x++)
				{
					terrain->SetLevel(x, y, levels[i++]);
				}
			}
			// The cells around the corners (a corner touches up to four).
			TerrainRect cells;
			cells.Min = glm::uvec2(m_Corners.Min.x > 0 ? m_Corners.Min.x - 1 : 0, m_Corners.Min.y > 0 ? m_Corners.Min.y - 1 : 0);
			cells.Max = glm::min(m_Corners.Max, glm::uvec2(terrain->GetCellCountX() - 1, terrain->GetCellCountY() - 1));
			terrain->MarkCellsChanged(cells);
		}
		if (m_HasCells)
		{
			const std::vector<uint8_t>& ramps = after ? m_RampsAfter : m_RampsBefore;
			const std::vector<uint8_t>& walls = after ? m_WallsAfter : m_WallsBefore;
			size_t i = 0;
			for (uint32_t y = m_Cells.Min.y; y <= m_Cells.Max.y; y++)
			{
				for (uint32_t x = m_Cells.Min.x; x <= m_Cells.Max.x; x++)
				{
					terrain->SetRamp(x, y, (TerrainRamp)ramps[i]);
					terrain->SetWallMaterial(x, y, walls[i]);
					i++;
				}
			}
			terrain->MarkCellsChanged(m_Cells);
		}
		if (m_HasTexels)
		{
			const std::vector<uint8_t>& weights = after ? m_WeightsAfter : m_WeightsBefore;
			const uint32_t width = m_Texels.Max.x - m_Texels.Min.x + 1;
			for (uint32_t y = m_Texels.Min.y; y <= m_Texels.Max.y; y++)
			{
				for (uint32_t x = m_Texels.Min.x; x <= m_Texels.Max.x; x++)
				{
					const size_t i = ((size_t)(y - m_Texels.Min.y) * width + (x - m_Texels.Min.x)) * 4;
					terrain->SetWeight(x, y, glm::u8vec4(weights[i], weights[i + 1], weights[i + 2], weights[i + 3]));
				}
			}
			terrain->MarkWeightsChanged(m_Texels);
		}
	}
}
