#include "hypch.h"

#include <vulkan/vulkan.h>
#include <imgui.h>

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

#include "../Scene/Components/Components.h"
#include "../Scene/Entity.h"

namespace Hydro
{
	void SceneRenderer::SetScene(const Ref<Scene>& scene)
	{
		m_Scene = scene;

		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			m_Scene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
	}

	void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;

		Renderer2D::SetViewportSize(width, height);
		m_Scene->OnViewportResize(width, height);
	}

	void SceneRenderer::RenderEditor(const EditorCamera& camera)
	{
		Renderer2D::BeginScene(camera.GetViewProjection());
		SubmitSprites();
	}

	bool SceneRenderer::RenderRuntime()
	{
		Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
		if ((bool)cameraEntity == false)
			return false;

		// The camera's position and rotation; scale would distort the view, so it's ignored.
		glm::mat4 cameraTransform(1.0f);
		if (cameraEntity.HasComponent<TransformComponent>())
		{
			const TransformComponent& transform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(glm::quat(transform.Rotation));
		}

		const SceneCamera& sceneCamera = cameraEntity.GetComponent<CameraComponent>().Camera;
		Renderer2D::BeginScene(Camera(sceneCamera.GetProjection(m_Scene->GetViewportAspectRatio())), cameraTransform);
		SubmitSprites();
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

	void SceneRenderer::RenderCompositeImage(float x, float y)
	{
		// Render the scene at the panel's size (applied next Render()), so the image is
		// never stretched and the framebuffer always matches what's displayed.
		if (x >= 1.0f && y >= 1.0f)
			SetViewportSize((uint32_t)x, (uint32_t)y);

		// Camera projections flip Y (Camera.h), so the image is already the right way up.
		auto image = Renderer2D::GetCompositeDescriptorSet();
		ImGui::Image(image, { x, y }, { 0, 0 }, { 1, 1 });
	}
}
