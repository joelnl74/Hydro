#pragma once
#include "EditorCommand.h"

#include <Hydro/Asset/Asset.h>
#include <Hydro/Terrain/Terrain.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// One brush stroke on a terrain's weight map (Terrain plan TR1: Paint): the texels it changed, before and after.
	// Refers to the terrain by its asset handle and applies to whatever object is loaded for it, so an undo after a
	// save (which reloads the asset) still edits the terrain that is drawn. TR2 adds levels and cells.
	class TerrainStrokeCommand : public EditorCommand
	{
	public:
		// `before` and `after` are the RGBA8 texels of `texels` (both corners included), rows from texels.Min.y.
		TerrainStrokeCommand(AssetHandle terrain, const TerrainRect& texels, std::vector<uint8_t> before, std::vector<uint8_t> after);

		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return "Paint Terrain"; }

	private:
		void Apply(const std::vector<uint8_t>& texels) const;

	private:
		AssetHandle m_Terrain = 0;
		TerrainRect m_Texels;
		std::vector<uint8_t> m_Before;
		std::vector<uint8_t> m_After;
	};
}
