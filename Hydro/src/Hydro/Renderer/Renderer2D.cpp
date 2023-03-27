#include <hypch.h>

#include <hypch.h>
#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"

#include "Hydro/Renderer/Renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Hydro
{
	struct QuadVertex
	{
		glm::vec2 Position;
		glm::vec3 Color;
	};

	struct UniformBufferObject 
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
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

		UniformBufferObject ubo;

		Ref<VulkanShader> QuadVertexShader;
		Ref<VulkanShader> QuadFragmentShader;
		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
		Ref<VulkanUniformBuffer> QuadUniformBuffer;
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DData();

		s_Data->QuadVertexShader = CreateRef<VulkanShader>();
		s_Data->QuadFragmentShader = CreateRef<VulkanShader>();
		s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>();

		s_Data->QuadVertexShader->Create("vertex.spv");
		s_Data->QuadFragmentShader->Create("fragment.spv");

		PipelineSpecification specification;
		specification.Layout =
		{
			{ ShaderDataType::Float2, "inPosition" },
			{ ShaderDataType::Float3, "inColor" },
		};

		specification.vertex = s_Data->QuadVertexShader;
		specification.fragment = s_Data->QuadFragmentShader;

		s_Data->QuadVertexShader->CreateDescriptorSetLayout();
		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);

		s_Data->QuadVertexBuffer = CreateRef<VulkanVertexBuffer>((void*)s_Data->vertices.data(), s_Data->vertices.size() * sizeof(s_Data->vertices[0]));
		s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>((void*)s_Data->indices.data(), s_Data->indices.size() * sizeof(s_Data->indices[0]));
		s_Data->QuadUniformBuffer->Create(sizeof(UniformBufferObject));

		s_Data->QuadVertexShader->CreateDescriptorPool();
		s_Data->QuadVertexShader->CreateDescriptorSet(s_Data->QuadUniformBuffer->GetVKBuffers(), sizeof(UniformBufferObject));
	}

	void Renderer2D::Begin()
	{
		uint32_t currentImage = Renderer::GetRenderFrame();
		auto extent = Renderer::GetVulkanPresentation()->GetExtend();

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		s_Data->ubo.model = glm::mat4(1);
		s_Data->ubo.view = glm::mat4(1);
		s_Data->ubo.proj = glm::mat4(1.0f);

		// s_Data->ubo.view = glm::translate(s_Data->ubo.view, glm::vec3(0.0f, 0.0f, -3.0f));
		// s_Data->ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
		// s_Data->ubo.proj[1][1] *= -1;



		s_Data->QuadUniformBuffer->Update(currentImage, &s_Data->ubo, sizeof(UniformBufferObject));
	}

	void Renderer2D::End()
	{
	}

	void Renderer2D::DrawQuad()
	{
		s_Data->QuadPipeline->Bind();
		s_Data->QuadVertexBuffer->Bind();
		s_Data->QuadIndexBuffer->Bind();
		auto commandBuffer = Renderer::GetVulkanPresentation()->GetCommandBuffer();

		s_Data->QuadPipeline->UpdateBuffers();
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
	}
}
