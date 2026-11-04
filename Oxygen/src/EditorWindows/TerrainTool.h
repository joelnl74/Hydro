#pragma once
#include <Hydro/Asset/Asset.h>
#include <Hydro/Renderer/EditorCamera.h>
#include <Hydro/Renderer/SceneRenderer.h>
#include <Hydro/Scene/Components/Components.h>
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Scene.h>
#include <Hydro/Terrain/Terrain.h>

#include "Terrain/TerrainBrushes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class EditorCommandStack;
	struct TerrainSnapshot;

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

	// Edits the selected entity's terrain asset with brushes in the Editor view; each stroke is one undo step.
	// The brush finds the ground by raycasting the terrain's triangles, so it works in 2D and 3D views alike.
	class TerrainTool
	{
	public:
		TerrainTool();
		~TerrainTool();

		// The inspector's Terrain section after the component's own rows: the asset, the brush and the terrain's
		// settings (applied at once, saved with the scene).
		void DrawInspector(Entity entity, TerrainComponent& component, bool playing);

		// Every frame in the Editor view: tracks the brush and edits while the left button is down. True while the tool
		// owns the mouse, so the caller skips click-to-select and the gizmo.
		bool UpdateViewport(Entity selected, const EditorCamera& camera, const glm::vec2& imageMin, const glm::vec2& imageSize,
			bool hovered, bool playing, float deltaTime, EditorCommandStack& commands);
		// Ends a stroke in progress, pushing its undo step.
		void EndStroke(EditorCommandStack& commands);

		// The brush circle and the Path brush's points for the Editor view's overlay.
		void FillOverlay(SceneRenderer::EditorOverlay& overlay) const;

		bool IsPainting() const { return m_Editing; }
		void SetPainting(bool editing) { m_Editing = editing; }

		// Writes every terrain asset `scene` uses that changed since it was loaded or saved.
		static void SaveModifiedTerrains(Scene& scene);

	private:
		void BeginStroke(const Ref<Terrain>& terrain, AssetHandle handle);
		// Paints m_Layer along the curve through m_PathPoints as one stroke.
		void ApplyPath(const Ref<Terrain>& terrain, AssetHandle handle, EditorCommandStack& commands);
		// A new .hyterrain in Assets/Terrains, named after the entity, assigned to `component`.
		void CreateTerrainAsset(Entity entity, TerrainComponent& component);

		void DrawBrush(const Ref<Terrain>& terrain);

	private:
		// Brush settings, for this session only.
		bool m_Editing = false;
		TerrainBrush m_Brush = TerrainBrush::Paint;
		TerrainBrushSettings m_BrushSettings;
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
