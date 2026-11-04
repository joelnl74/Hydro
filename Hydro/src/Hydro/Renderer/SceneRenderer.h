#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/SceneSpriteRenderer.h"
#include "Hydro/Renderer/SceneUIRenderer.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Texture2D.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class Gizmos;

	// Submits a scene to Renderer2D and Renderer3D through a camera, into a RenderView: the camera, meshes, terrain,
	// lights and shadows here; sprites, the UI and the Editor view's lines in SceneSprite-, SceneUI- and SceneEditorRenderer.
	class SceneRenderer
	{
	public:
		// Editor-only lines over the scene (never lit): the selection and the colliders.
		struct EditorOverlay
		{
			Entity Selected;                 // outlined when SelectionOutline is on
			bool SelectionOutline = true;
			bool Colliders = false;          // every collider: sensor yellow, static green, dynamic blue, kinematic cyan
			// The Terrain tool's brush: a circle on the terrain where it would paint.
			bool TerrainBrush = false;
			glm::vec3 TerrainBrushCentre{ 0.0f };
			float TerrainBrushRadius = 1.0f;
			glm::vec4 TerrainBrushColor{ 1.0f, 1.0f, 1.0f, 0.9f };
			// The Path brush's points so far (world space), joined by a line; empty when there are none.
			std::vector<glm::vec3> TerrainPath;
			// Off keeps the Editor view sharp. PostProcessing: depth of field, bloom and vignette (exposure and tonemap stay);
			// Fog: the environment's fog.
			bool PostProcessing = false;
			bool Fog = false;
			// What scripts draw with OnDrawGizmos: lines over the scene; Oxygen draws the labels. Not owned.
			const Gizmos* ScriptGizmos = nullptr;
		};

		SceneRenderer(const Ref<Scene>& scene)
			: m_Scene(scene) {
		}

		void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }
		const Ref<Scene>& GetScene() const { return m_Scene; }

		// Editing: the scene seen through the editor's camera.
		void RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay);
		// An overload, not a default argument: g++ can't default a nested type with member initialisers.
		void RenderEditor(RenderView& view, const EditorCamera& camera);
		// The scene through its primary camera, with its UI canvases on top. Keep Scene::OnViewportResize in step
		// with the view's size. Without a primary camera it draws only the UI and returns false.
		bool RenderRuntime(RenderView& view);

		// The scene's render features when it overrides the project's, else RendererSettings::Features.
		RenderFeatureSettings GetEffectiveRenderFeatures() const;

		// The direction an entity's -Z points to (spot lights, the sun): its full 3D rotation.
		static glm::vec3 GetForwardDirection(const WorldTransformComponent& transform);

	private:
		// Moves the camera to whole screen pixels of a `viewHeight` view, measured at its focus point (#136).
		// Only the drawn view moves; the camera entity is left as it is.
		static void SnapCameraToPixels(glm::mat4& cameraTransform, const SceneCamera& camera, uint32_t viewHeight);

		// Meshes and terrains (Renderer3D) when the render features draw meshes; warns once per scene otherwise.
		void SubmitMeshes(RenderView& renderView);
		// Called inside SubmitMeshes' Renderer3D scene.
		void SubmitTerrains();
		// A point light's Height moves it up along the camera's up, like a point on an Upright sprite.
		void SubmitLights();
		// The sun, ambient, fog and the point and spot lights, handed to Renderer2D::SetSceneLighting.
		void SubmitForwardLighting();
		// Blob shadows (Shadow2DComponents) in lit views.
		void SubmitShadows();

	private:
		Ref<Scene> m_Scene;
		SceneCameraBasis m_Camera;
		bool m_Fog = true; // the scene's fog in this frame (off in the Editor view unless EditorOverlay::Fog)
		SceneSpriteRenderer m_Sprites;
		SceneUIRenderer m_UI;
		bool m_MeshWarningShown = false;
	};
}
