#include "TerrainStrokeCommand.h"

#include <Hydro/Asset/AssetManager.h>

namespace Hydro
{
	TerrainStrokeCommand::TerrainStrokeCommand(AssetHandle terrain, const TerrainRect& texels, std::vector<uint8_t> before, std::vector<uint8_t> after)
		: m_Terrain(terrain), m_Texels(texels), m_Before(std::move(before)), m_After(std::move(after))
	{
	}

	void TerrainStrokeCommand::Undo()
	{
		Apply(m_Before);
	}

	void TerrainStrokeCommand::Redo()
	{
		Apply(m_After);
	}

	void TerrainStrokeCommand::Apply(const std::vector<uint8_t>& texels) const
	{
		const Ref<Terrain> terrain = AssetManager::GetLoadedAsset<Terrain>(m_Terrain);
		if (terrain == nullptr)
		{
			return;
		}

		const uint32_t width = m_Texels.Max.x - m_Texels.Min.x + 1;
		const uint32_t height = m_Texels.Max.y - m_Texels.Min.y + 1;
		if (m_Texels.Max.x >= terrain->GetWeightMapWidth() || m_Texels.Max.y >= terrain->GetWeightMapHeight()
			|| texels.size() != (size_t)width * height * 4)
		{
			return; // the terrain was replaced by one of another size
		}

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const size_t i = ((size_t)y * width + x) * 4;
				terrain->SetWeight(m_Texels.Min.x + x, m_Texels.Min.y + y, glm::u8vec4(texels[i], texels[i + 1], texels[i + 2], texels[i + 3]));
			}
		}
		terrain->MarkWeightsChanged(m_Texels);
	}
}
