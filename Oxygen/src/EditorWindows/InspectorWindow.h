#pragma once
#include "TextureImportPanel.h"
#include "Inspector/DataAssetInspector.h"
#include "Inspector/MaterialEditor.h"

#include "Hydro/Core/UUID.h"
#include "Hydro/Renderer/MeshImporter.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/SpriteAnimation.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Hydro
{
	class TerrainTool;

	// Shows and edits the selected entity's components, or, with no entity selected, the asset selected in the
	// Project window. Editor UI only: it holds no behaviour of its own. The sections are in Inspector/.
	class InspectorWindow
	{
	public:
		std::function<void(const std::filesystem::path&)> OnTextureReimported;
		std::function<void(const std::filesystem::path&)> OnOpenSpriteEditor;
		std::function<void(const std::filesystem::path&)> OnOpenAnimation;
		// An entity clicked in the Used by list.
		std::function<void(Entity)> OnSelectEntity;

		InspectorWindow() = default;
		~InspectorWindow() = default;

		// `asset` is shown when `entity` isn't valid (empty: nothing).
		void OnImguiRender(Entity entity, const std::filesystem::path& asset);

		// Draws the Terrain section's asset, layers and brush (owned by EditorLayer; may be null).
		void SetTerrainTool(TerrainTool* tool) { m_TerrainTool = tool; }
		// While playing, some edits (painting a terrain asset shared with the editor scene) wait for Stop.
		void SetPlaying(bool playing) { m_Playing = playing; }

	private:
		void DrawEntityHeader(Entity entity);
		// The entities whose fields or script properties refer to this one (a route's walkers, a button's neighbours).
		void DrawUsedBy(Entity entity);
		void DrawAddComponentButton(Entity entity);
		void DrawPrefabButton(Entity entity);
		void DrawAsset(const std::filesystem::path& asset);
		void DrawAnimationSetAsset(const std::filesystem::path& path);
		void DrawMaterialAsset(const std::filesystem::path& path);
		// A selected .hyrender's environment and post-processing, shared by every scene using it.
		void DrawRenderProfileAsset(const std::filesystem::path& path);
		// A selected .gltf / .glb: its import settings (Apply re-imports) and contents.
		void DrawModelAsset(const std::filesystem::path& path);
		// Writes what was edited last (materials, data, render profiles) once no field is active.
		void SaveEdits();

	private:
		// Who refers to the selected entity, found again when the selection changes and twice a second.
		struct Referrer
		{
			UUID Source = 0;
			std::string Where;
		};

		TextureImportPanel m_TextureImportPanel;
		MaterialEditor m_Materials;
		DataAssetInspector m_DataAssets;
		// Held so the shared set isn't loaded again every frame.
		Ref<SpriteAnimationSet> m_InspectedAnimationSet;
		std::filesystem::path m_InspectedAnimationPath;
		std::filesystem::file_time_type m_InspectedAnimationTime{};
		// The import settings being edited for the selected model (read again when another one is selected).
		std::filesystem::path m_ModelSettingsPath;
		MeshImportConfig m_ModelSettings;
		std::vector<Referrer> m_Referrers;
		UUID m_ReferrersOf = 0;
		double m_ReferrersTime = -1.0;
		TerrainTool* m_TerrainTool = nullptr;
		bool m_Playing = false;
	};
}
