#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"

namespace Hydro
{
	// Submits a scene to Renderer2D (render submission, step 4 of the frame order), into a RenderView
	// and through a camera.
	class SceneRenderer
	{
	public:
		SceneRenderer(const Ref<Scene>& scene)
			: m_Scene(scene) {
		}

		void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }
		const Ref<Scene>& GetScene() const { return m_Scene; }

		// Editing: the scene seen through the editor's camera.
		void RenderEditor(RenderView& view, const EditorCamera& camera);
		// Runtime: the scene seen through its primary CameraComponent, with the scene's viewport
		// aspect ratio (keep Scene::OnViewportResize in step with the view's size). Draws nothing and
		// returns false when the scene has no primary camera; the view is still cleared.
		bool RenderRuntime(RenderView& view);

	private:
		void SubmitSprites();

	private:
		Ref<Scene> m_Scene;
	};
}
