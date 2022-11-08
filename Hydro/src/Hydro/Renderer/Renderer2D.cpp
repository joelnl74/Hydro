#include <hypch.h>

#include <hypch.h>
#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanShader.h"

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

		struct UniformBufferObject {
			glm::mat4 viewProjectionMatrix;
		};

		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
		Ref<VulkanUniformBuffer> QuadUniformBuffer;
		Ref<VulkanShader> VertexShader;
		Ref<VulkanShader> FragmentShader;

		UniformBufferObject UBOdata;
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

		s_Data->VertexShader = CreateRef<VulkanShader>();
		s_Data->FragmentShader = CreateRef<VulkanShader>();

		auto vertexShader = s_Data->VertexShader->Create("vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
		auto fragmentShader = s_Data->FragmentShader->Create("fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		s_Data->QuadVertexBuffer = CreateRef<VulkanVertexBuffer>((void*)s_Data->vertices.data(), s_Data->vertices.size() * sizeof(QuadVertex));
		s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>((void*)s_Data->indices.data(), s_Data->indices.size() * sizeof(uint16_t));
		s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>(sizeof(s_Data->UBOdata));

		specification.VertexShader = s_Data->VertexShader;
		specification.FragmentShader = s_Data->FragmentShader;

		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);


	}

	void Renderer2D::ShutDown()
	{
		// TODO Clean up.
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
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
	}
}
