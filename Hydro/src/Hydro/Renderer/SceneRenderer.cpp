#include "hypch.h"

#include <vulkan/vulkan.h>
#include <imgui.h>


#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

namespace Hydro
{
	void SceneRenderer::RenderScene()
	{

		for (size_t i = 0; i < 24; i++)
		{
			for (size_t j = 0; j < 24; j++)
			{
				glm::mat4 transform = glm::mat4(1);
				transform = glm::translate(transform, glm::vec3(j * 64, i * 64, 0));
				transform = glm::scale(transform, glm::vec3(64, 64, 0));

				Renderer2D::DrawQuad(transform, glm::vec4(j * 0.05, i * 0.05, 1, 1));
			}
		}
	}

	
	void SceneRenderer::RenderCompositeImage(float x, float y)
	{
		HY_CORE_INFO("Composite image generation.");
		auto image = Renderer2D::GetCompositeDescriptorSet();
		ImGui::Image(image, {x, y }, { 0, 1 }, { 1, 0 });
		HY_CORE_INFO("Composite image generated successfully.");
	}
}
