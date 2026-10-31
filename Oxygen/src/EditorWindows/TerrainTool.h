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
	struct TerrainSnapshot;

	// The Terrain tool's brushes (Terrain plan, "Editing").
	enum class TerrainBrush : uint8_t
	{
		Paint = 0, // raise one layer's weight; Shift lowers it
		Raise,     // corners under the brush up one level, once per stroke; Shift lowers
		Lower,     // corners down one level, once per stroke; Shift raises
		Flatten,   // corners to the level under the first click
		Wall,      // the wall layer (or Hidden) of the cells under the brush
		Ramp,      // drag from the low side to the high side: the cells' ramp direction; Shift clears
		Path       // click points, Enter paints a layer along the curve through them with wandering edges
	};

	// The Terrain tool: edits the terrain asset of the selected entity's TerrainComponent with brushes in the Editor
	// view. Each stroke is one undo step (TerrainStrokeCommand). Its settings and the terrain's own settings are drawn
	// in the inspector's Terrain section. Owned by EditorLayer.
	//
	// The brush finds the ground with a ray from the editor camera through the cursor against the terrain's triangles
	// (TerrainCache::Raycast), so it works in the 2D and 3D views alike.
	class TerrainTool
	{
	public:
		TerrainTool();
		~TerrainTool();

		// The inspector's Terrain section, after the component's own rows: the asset (or Create Terrain), the brush, and
		// the terrain's settings (layers, walls, edges, cap stones, decor; changes apply at once, saved with the scene).
		void DrawInspector(Entity entity, TerrainComponent& component, bool playing);

		// The Editor view, every frame: where the brush is, and editing while the left button is down. `imageMin` and
		// `imageSize` are the view's image in ImGui pixels. True while the tool owns the mouse in the view (editing is on
		// and a terrain is selected): the caller then skips click-to-select and the gizmo.
		bool UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize, bool hovered,
			bool playing, float deltaTime, EditorCommandStack& commands);
		// Ends a stroke in progress (pushing its undo step), e.g. before Play or another scene.
		void EndStroke(EditorCommandStack& commands);

		// The brush circle and the Path brush's points for the Editor view's overlay.
		void FillOverlay(SceneRenderer::EditorOverlay& overlay) const;

		bool IsPainting() const { return m_Editing; }
		void SetPainting(bool editing) { m_Editing = editing; }

		// Writes every terrain asset `scene` uses that changed since it was loaded or saved (EditorLayer::SaveScene).
		static void SaveModifiedTerrains(Scene& scene);

	private:
		void BeginStroke(const Ref<Terrain>& terrain, AssetHandle handle);
		// One dab of the Paint brush at `centre` (the terrain's own space, metres) for `deltaTime` seconds.
		void Paint(Terrain& terrain, const glm::vec2& centre, bool lower, float deltaTime);
		// The level brushes: Raise / Lower (`step` +1 or -1, each corner once per stroke) and Flatten (to m_FlattenLevel).
		void ChangeLevels(Terrain& terrain, const glm::vec2& centre, int32_t step, bool flatten);
		// The Wall brush: the cells under the brush get m_WallLayer.
		void PaintWalls(Terrain& terrain, const glm::vec2& centre);
		// The Ramp brush: the cells under the brush get the direction from the stroke's start to `centre` (or none).
		void PaintRamps(Terrain& terrain, const glm::vec2& centre, bool clear);
		// The Path brush: paints m_Layer along the curve through m_PathPoints, as one stroke.
		void ApplyPath(const Ref<Terrain>& terrain, AssetHandle handle, EditorCommandStack& commands);
		// A new .hyterrain in Assets/Terrains, named after the entity, assigned to `component`.
		void CreateTerrainAsset(Entity entity, TerrainComponent& component);

		void DrawBrush(const Ref<Terrain>& terrain);
		void DrawTerrainSettings(const Ref<Terrain>& terrain);

	private:
		// Brush settings (the session's, not saved).
		bool m_Editing = false;
		TerrainBrush m_Brush = TerrainBrush::Paint;
		uint32_t m_Layer = 1;      // Paint and Path: the layer
		uint8_t m_WallLayer = 0;   // Wall: 0..3 or TerrainWallHidden
		float m_Radius = 1.0f;     // metres
		float m_Strength = 0.5f;   // Paint: how fast a dab reaches full weight (0..1)
		float m_Hardness = 0.5f;   // Paint: share of the radius at full strength; the rest fades out
		float m_PathWidth = 1.6f;  // Path: metres
		float m_PathWander = 0.15f; // Path: how far the edges wander in and out, metres
		glm::vec2 m_NewTerrainSize{ 32.0f, 32.0f }; // metres, for Create Terrain

		// Under the cursor.
		bool m_HasHit = false;
		glm::vec3 m_HitWorld{ 0.0f };
		glm::vec3 m_Origin{ 0.0f }; // the selected terrain entity's position
		bool m_Shift = false;       // Shift held: the circle shows it

		// The Path brush's points (the terrain's space) and the terrain they are on.
		std::vector<glm::vec3> m_PathPoints;
		AssetHandle m_PathTerrain = 0;

		// The stroke in progress.
		bool m_Stroking = false;
		AssetHandle m_StrokeTerrain = 0;
		TerrainBrush m_StrokeBrush = TerrainBrush::Paint;
		Scope<TerrainSnapshot> m_StrokeBefore;
		std::vector<float> m_StrokeWeights;  // Paint: the weights as the stroke builds them (bytes would lose small dabs)
		std::vector<uint8_t> m_StrokeTouched; // Raise / Lower: corners already moved this stroke
		int8_t m_FlattenLevel = 0;
		glm::vec2 m_StrokeStart{ 0.0f };
	};
}
