#pragma once
#include "EditorCommand.h"

#include <Hydro/Asset/Asset.h>
#include <Hydro/Terrain/Terrain.h>

namespace Hydro
{
	// A change to a terrain's own settings (layers, walls, edges, decor) in the inspector, by asset handle so an undo
	// after a save (which reloads it) still edits the drawn terrain.
	class TerrainSettingsCommand : public EditorCommand
	{
	public:
		TerrainSettingsCommand(AssetHandle terrain, TerrainSettings before, TerrainSettings after);

		void Undo() override { Apply(m_Before); }
		void Redo() override { Apply(m_After); }
		const char* GetName() const override { return "Edit Terrain Settings"; }
		size_t GetMemorySize() const override { return sizeof(*this); }

	private:
		void Apply(const TerrainSettings& settings) const;

	private:
		AssetHandle m_Terrain = 0;
		TerrainSettings m_Before;
		TerrainSettings m_After;
	};
}
