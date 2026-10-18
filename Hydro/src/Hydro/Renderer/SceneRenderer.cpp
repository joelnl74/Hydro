#include "hypch.h"

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

#include "../Scene/Components/Components.h"
#include "../Scene/Entity.h"

namespace Hydro
{
	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera)
	{
		Renderer2D::BeginScene(view, camera.GetViewProjection());
		SubmitSprites();
		Renderer2D::EndScene();
	}

	bool SceneRenderer::RenderRuntime(RenderView& view)
	{
		Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
		if ((bool)cameraEntity == false)
		{
			// Nothing to look through: begin the view anyway so it is cleared, not left stale.
			Renderer2D::BeginScene(view, glm::mat4(1.0f));
			Renderer2D::EndScene();
			return false;
		}

		// The camera's position and rotation; scale would distort the view, so it's ignored.
		glm::mat4 cameraTransform(1.0f);
		if (cameraEntity.HasComponent<TransformComponent>())
		{
			const TransformComponent& transform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(glm::quat(transform.Rotation));
		}

		const SceneCamera& sceneCamera = cameraEntity.GetComponent<CameraComponent>().Camera;
		Renderer2D::BeginScene(view, Camera(sceneCamera.GetProjection(m_Scene->GetViewportAspectRatio())), cameraTransform);
		SubmitSprites();
		Renderer2D::EndScene();
		return true;
	}

	void SceneRenderer::SubmitSprites()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
		}

		// TODO MESH RENDERING
	}
}
