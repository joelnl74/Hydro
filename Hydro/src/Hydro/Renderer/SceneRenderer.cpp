#include "hypch.h"

#include <vulkan/vulkan.h>
#include <imgui.h>

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

#include "../Scene/Components//Components.h"
#include "../Scene/Entity.h"

namespace Hydro
{
	void SceneRenderer::RenderScene()
	{
		// Sprite rendering.
		{
			auto group = m_Scene->m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
			}
		}

		// TODO MESH RENDERING
	}

	
	void SceneRenderer::RenderCompositeImage(float x, float y)
	{
		auto image = Renderer2D::GetCompositeDescriptorSet();
		ImGui::Image(image, {x, y }, { 0, 1 }, { 1, 0 });
	}
}
