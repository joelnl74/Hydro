#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Renderer/EditorCamera.h"

#include <cstdint>

namespace Hydro
{
	// Submits a scene to Renderer2D (render submission, step 3 of the frame order). Pick one Render
	// call per frame: there is one 2D pass, so the last BeginScene of the frame wins.
	class SceneRenderer
	{
	public:
		SceneRenderer(const Ref<Scene>& scene)
			: m_Scene(scene) {
		}

		// Also passes the current viewport size on to the new scene.
		void SetScene(const Ref<Scene>& scene);

		// The size the scene is rendered at: resizes Renderer2D's target and the scene's viewport
		// (camera aspect ratios). 0 is ignored.
		void SetViewportSize(uint32_t width, uint32_t height);

		// Editing: the scene seen through the editor's camera.
		void RenderEditor(const EditorCamera& camera);
		// Runtime: the scene seen through its primary CameraComponent. Draws nothing and returns false
		// when the scene has no primary camera.
		bool RenderRuntime();

		// Shows the rendered image in the current ImGui window at the given size, and renders at that
		// size from the next frame on (SetViewportSize).
		void RenderCompositeImage(float x, float y);

	private:
		void SubmitSprites();

	private:
		Ref<Scene> m_Scene;
		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
	};
}
