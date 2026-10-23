#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"

#include <cstdint>
#include <vector>

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
		// Sorts the scene's sprites (SpriteRendererComponent::SortOrder, then Z, then storage order)
		// and submits them back to front.
		void SubmitSprites();
		// One sprite: the whole texture (tiled), or a sprite of a sheet placed by its pivot; flipped as asked.
		static void SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite);

	private:
		struct SpriteDrawItem
		{
			int32_t SortOrder = 0;
			float Z = 0.0f;
			uint32_t Index = 0; // storage order, so equal keys keep a stable order
			entt::entity Entity = entt::null;
		};

		Ref<Scene> m_Scene;
		// Reused every frame (capacity kept), so sorting allocates nothing once the scene is drawn.
		std::vector<SpriteDrawItem> m_SpriteQueue;
	};
}
