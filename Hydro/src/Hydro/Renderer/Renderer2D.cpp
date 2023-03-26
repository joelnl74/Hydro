#include <hypch.h>

#include <hypch.h>
#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"

#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	struct QuadVertex
	{
		glm::vec2 Position;
		glm::vec3 Color;
	};

	struct Renderer2DData
	{
		const std::vector<QuadVertex> vertices = {
			{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
		};

		const std::vector<uint16_t> indices = {
			0, 1, 2, 2, 3, 0
		};

		Ref<VulkanShader> QuadVertexShader;
		Ref<VulkanShader> QuadFragmentShader;
		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DData();

		s_Data->QuadVertexShader = CreateRef<VulkanShader>();
		s_Data->QuadFragmentShader = CreateRef<VulkanShader>();

		auto vertexShader = s_Data->QuadVertexShader->Create("vertex.spv");
		auto fragmentShader = s_Data->QuadFragmentShader->Create("fragment.spv");

		s_Data->QuadVertexShader->CreateDescriptorSetLayout();

		PipelineSpecification specification;
		specification.Layout = 
		{
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float3, "a_Color" },
		};

		specification.vertex = s_Data->QuadVertexShader;
		specification.fragment = s_Data->QuadFragmentShader;

		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);
		s_Data->QuadVertexBuffer = CreateRef<VulkanVertexBuffer>((void*)s_Data->vertices.data(), s_Data->vertices.size() * sizeof(QuadVertex));
		s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>((void*)s_Data->indices.data(), s_Data->indices.size() * sizeof(uint16_t));
	}

	void Renderer2D::Begin()
	{
		s_Data->QuadPipeline->Bind();
		s_Data->QuadVertexBuffer->Bind();
		s_Data->QuadIndexBuffer->Bind();
	}

	void Renderer2D::End()
	{
	}

	void Renderer2D::DrawQuad()
	{
		auto commandBuffer = Renderer::GetVulkanPresentation()->GetCommandBuffer();

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
	}
}
