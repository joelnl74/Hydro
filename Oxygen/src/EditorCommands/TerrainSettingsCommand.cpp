#include "TerrainSettingsCommand.h"

#include <Hydro/Asset/AssetManager.h>

namespace Hydro
{
	TerrainSettingsCommand::TerrainSettingsCommand(AssetHandle terrain, TerrainSettings before, TerrainSettings after)
		: m_Terrain(terrain), m_Before(std::move(before)), m_After(std::move(after))
	{
	}

	void TerrainSettingsCommand::Apply(const TerrainSettings& settings) const
	{
		const Ref<Terrain> terrain = AssetManager::GetLoadedAsset<Terrain>(m_Terrain);
		if (terrain != nullptr)
		{
			terrain->SetSettings(settings);
		}
	}
}
