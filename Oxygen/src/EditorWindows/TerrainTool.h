#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Renderer/EditorCamera.h>
#include <Hydro/Renderer/SceneRenderer.h>
#include <Hydro/Scene/Components/Components.h>
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Terrain/Terrain.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class EditorCommandStack;

	// The Terrain tool (Terrain plan, "Editing"): edits the terrain asset of the selected entity's TerrainComponent
	// with brushes in the Editor view. TR1 has the Paint brush: left-drag raises one layer's weight, Shift+left-drag
	// lowers it; each stroke is one undo step (TerrainStrokeCommand). Its settings and the terrain's own settings are
	// drawn in the inspector's Terrain section. Owned by EditorLayer.
	//
	// The brush finds the ground with a ray from the editor camera through the cursor against the terrain's triangles
	// (TerrainCache::Raycast), so it works in the 2D and 3D views alike.
	class TerrainTool
	{
	public:
		// The inspector's Terrain section, after the component's own rows: the asset (or Create Terrain), the terrain's
		// layers and edge noise (changes apply at once and are saved with the scene), and the brush.
		void DrawInspector(Entity entity, TerrainComponent& component, bool playing);

		// The Editor view, every frame: where the brush is, and painting while the left button is down. `imageMin` and
		// `imageSize` are the view's image in ImGui pixels. True while the tool owns the mouse in the view (painting is on
		// and a terrain is selected): the caller then skips click-to-select and the gizmo.
		bool UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize, bool hovered,
			bool playing, float deltaTime, EditorCommandStack& commands);
		// Ends a stroke in progress (pushing its undo step), e.g. before Play or another scene.
		void EndStroke(EditorCommandStack& commands);

		// The brush circle for the Editor view's overlay.
		void FillOverlay(SceneRenderer::EditorOverlay& overlay) const;

		bool IsPainting() const { return m_Painting; }
		void SetPainting(bool painting) { m_Painting = painting; }

		// Writes every terrain asset `scene` uses that changed since it was loaded or saved (EditorLayer::SaveScene).
		static void SaveModifiedTerrains(Scene& scene);

	private:
		void BeginStroke(const Ref<Terrain>& terrain, AssetHandle handle);
		// One dab of the brush at `centre` (the terrain's own space, metres) for `deltaTime` seconds.
		void Paint(Terrain& terrain, const glm::vec2& centre, bool lower, float deltaTime);
		// A new .hyterrain in Assets/Terrains, named after the entity, assigned to `component`.
		void CreateTerrainAsset(Entity entity, TerrainComponent& component);

	private:
		// Brush settings (the session's, not saved).
		bool m_Painting = false;
		uint32_t m_Layer = 1;      // the layer the brush raises
		float m_Radius = 1.0f;     // metres
		float m_Strength = 0.5f;   // how fast a dab reaches full weight (0..1)
		float m_Hardness = 0.5f;   // share of the radius at full strength; the rest fades out
		glm::vec2 m_NewTerrainSize{ 32.0f, 32.0f }; // metres, for Create Terrain

		// Under the cursor.
		bool m_HasHit = false;
		glm::vec3 m_HitWorld{ 0.0f };
		bool m_Lowering = false; // Shift held: the circle shows it

		// The stroke in progress.
		bool m_Stroking = false;
		bool m_StrokeChanged = false;
		AssetHandle m_StrokeTerrain = 0;
		std::vector<uint8_t> m_StrokeBefore; // the whole weight map when the stroke began
		std::vector<float> m_StrokeWeights;  // the weights as the stroke builds them (bytes would lose small dabs)
		TerrainRect m_StrokeTexels;           // what the stroke changed
	};
}
