#pragma once
#include "EditorCommand.h"

#include <Hydro/Asset/Asset.h>
#include <Hydro/Terrain/Terrain.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A terrain's data at one moment (the start of a stroke), for TerrainStrokeCommand.
	struct TerrainSnapshot
	{
		std::vector<int8_t> Levels;
		std::vector<uint8_t> Ramps;
		std::vector<uint8_t> WallMaterials;
		std::vector<uint8_t> Weights;

		static TerrainSnapshot Take(const Terrain& terrain);
	};

	// One brush stroke on a terrain (Terrain plan, "Editing"): the corners, cells and weight texels it changed, before
	// and after, each kept only over the rectangle that changed. Refers to the terrain by its asset handle and applies
	// to whatever object is loaded for it, so an undo after a save (which reloads the asset) still edits the terrain
	// that is drawn.
	class TerrainStrokeCommand : public EditorCommand
	{
	public:
		// What changed between `before` and `after` (the terrain now). `name` is what the Edit menu says.
		TerrainStrokeCommand(AssetHandle terrain, std::string name, const TerrainSnapshot& before, const Terrain& after);

		// Nothing changed: don't push it.
		bool IsEmpty() const { return m_HasCorners == false && m_HasCells == false && m_HasTexels == false; }

		void Undo() override;
		void Redo() override;
		const char* GetName() const override { return m_Name.c_str(); }

	private:
		// The values of `rect` in a row-major array `width` wide, `channels` values per element.
		template<typename T>
		static std::vector<T> Cut(const std::vector<T>& values, uint32_t width, const TerrainRect& rect, uint32_t channels);
		void Apply(bool after) const;

	private:
		AssetHandle m_Terrain = 0;
		std::string m_Name;
		bool m_HasCorners = false;
		bool m_HasCells = false;
		bool m_HasTexels = false;
		TerrainRect m_Corners;
		TerrainRect m_Cells;
		TerrainRect m_Texels;
		std::vector<int8_t> m_LevelsBefore, m_LevelsAfter;
		std::vector<uint8_t> m_RampsBefore, m_RampsAfter;
		std::vector<uint8_t> m_WallsBefore, m_WallsAfter;
		std::vector<uint8_t> m_WeightsBefore, m_WeightsAfter;
	};
}
