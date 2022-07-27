#include <hypch.h>

#include <hypch.h>
#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"

namespace Hydro
{
	struct QuadVertex
	{
		glm::vec2 Position;
		glm::vec3 Color;
	};

	struct Renderer2DData
	{
		const std::vector<QuadVertex> vertices = 
		{
			{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
		};

		Ref<VulkanPipeline> QuadPipeline;
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DData();

		PipelineSpecification specification;
		specification.Layout = 
		{
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float3, "a_Color" },
		};

		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);
	}

	void Renderer2D::Begin()
	{
		s_Data->QuadPipeline->Bind();
	}

	void Renderer2D::End()
	{
	}

	void Renderer2D::DrawQuad()
	{
	}
}
