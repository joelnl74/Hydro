#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Core/Base.h>
#include <Hydro/Terrain/Terrain.h>

#include <string>

namespace Hydro
{
	// The inspector rows of a terrain asset's own settings (layers, walls, edges, cap stones, decor, edge noise),
	// edited on a copy and applied when something changed; the scene's save writes them.
	class TerrainSettingsPanel
	{
	public:
		static void Draw(const Ref<Terrain>& terrain);

		// Also used by the Terrain tool's Brush section. A section title: a separator and a label (the vendored ImGui
		// has no SeparatorText).
		static void DrawSection(const char* title);
		// The file name of an asset ("FarmDirt"), "none" or "missing".
		static std::string GetAssetName(AssetHandle handle);
	};
}
