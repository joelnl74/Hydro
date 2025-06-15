#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"
#include "Hydro/Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanRenderPass.h"

#include "Hydro/Renderer/Renderer.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Renderer/Camera.h"

namespace Hydro
{
	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 texCoord;
		float TexIndex;
	};

	struct UniformBufferObject 
	{
		alignas(16) glm::mat4 mvp;
	};

	struct Renderer2DData
	{
		static const uint32_t MaxQuads = 200000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO QUERY FROM GPU

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		glm::vec4 QuadVertexPositions[4];

		const std::vector<QuadVertex> vertices = 
		{
			{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
			{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
		};

		const std::vector<uint16_t> indices = 
		{
			0, 1, 2, 2, 3, 0
		};

		Ref<Camera> camera;
		UniformBufferObject ubo;

		Ref<VulkanShader> Shader;

		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
		Ref<VulkanUniformBuffer> QuadUniformBuffer;
		Ref<VulkanDescriptorBuilder> QuadDescriptorSet;


		Ref<VulkanRenderCommandBuffer> CommandBuffer;;
		Ref<VulkanRenderPass> RenderPass;;

		std::array<Ref<VullkanTexture>, MaxTextureSlots> TextureSlots;
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init()
	{
		auto& extent = Renderer::GetVulkanSwapChain()->GetExtend();

		s_Data = new Renderer2DData();
		s_Data->camera = CreateRef<Camera>(0, extent.width, 0, extent.height);

		/// Should be done before even entering the renderer shaders should have an get method from some sort of library.
		ShaderInformation vertexInformation;
		vertexInformation.path = "assets/shaders/vertex.spv";
		vertexInformation.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexInformation.shaderType = ShaderType::Vertex;

		ShaderInformation fragmentInformation;
		fragmentInformation.path = "assets/shaders/fragment.spv";
		fragmentInformation.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentInformation.shaderType = ShaderType::Fragment;

		ShaderSpecification shaderSpecification;
		shaderSpecification.shaderInformation =
		{
			vertexInformation,
			fragmentInformation,
		};

		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = { -0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { 0.5f, -0.5f, 0.0f, 1.0f };

		s_Data->Shader = CreateRef<VulkanShader>(shaderSpecification);
		/// END BLOCK!

		VulkanTextureSpecification textureProperties;

		s_Data->TextureSlots[0] = CreateRef<VullkanTexture>("assets/textures/white.png", textureProperties);

		s_Data->QuadVertexBuffer = CreateRef<VulkanVertexBuffer>(s_Data->MaxVertices * sizeof(s_Data->vertices[0]));
		s_Data->QuadVertexBufferBase = new QuadVertex[s_Data->MaxVertices];

		uint32_t* quadIndices = new uint32_t[s_Data->MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < s_Data->MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>(quadIndices, s_Data->MaxIndices * sizeof(s_Data->indices[0]));
		delete[] quadIndices;
		
		s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>((uint8_t)sizeof(UniformBufferObject));

		s_Data->QuadDescriptorSet = CreateRef<VulkanDescriptorBuilder>();
		s_Data->QuadDescriptorSet->Begin();
		s_Data->QuadDescriptorSet->BindBuffer(0, s_Data->QuadUniformBuffer, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		
		s_Data->QuadDescriptorSet->BindImage(1, s_Data->TextureSlots[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		s_Data->QuadDescriptorSet->Build();

		PipelineSpecification specification;
		specification.Layout =
		{
			{ ShaderDataType::Float3, "inPosition" },
			{ ShaderDataType::Float4, "inColor" },
			{ ShaderDataType::Float2, "inTexCoord" },
			{ ShaderDataType::Float, "inTexIndex" },
		};

		specification.descriptorSet = s_Data->QuadDescriptorSet;
		specification.shader = s_Data->Shader;

		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);
		s_Data->QuadIndexCount = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase;

		// Command Buffer and FrameBuffer rendere target for renderer 2D.
		s_Data->CommandBuffer = CreateRef<VulkanRenderCommandBuffer>(2, "2DRenderer");
		s_Data->RenderPass = CreateRef<VulkanRenderPass>();
	}

	void Renderer2D::ShutDown()
	{
		for (int i = 0; i < s_Data->TextureSlots.size(); i++)
		{
			if (s_Data->TextureSlots[i])
			{
				s_Data->TextureSlots[i]->Destroy();
			}
		}

		s_Data->QuadDescriptorSet->Destroy();
		s_Data->Shader->Destroy();
		s_Data->QuadPipeline->Destroy();
		s_Data->QuadUniformBuffer->Destroy();
		s_Data->QuadIndexBuffer->Destroy();
		s_Data->QuadVertexBuffer->Destroy();

		s_Data = nullptr;
	}

	void Renderer2D::Begin()
	{
		auto& extent = Renderer::GetVulkanSwapChain()->GetExtend();

		s_Data->CommandBuffer->Begin();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = s_Data->RenderPass->GetRenderPass();
		renderPassInfo.framebuffer = s_Data->RenderPass->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extent;

		VkClearValue clearColor = { {{1.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(s_Data->CommandBuffer->GetActiveCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		uint32_t currentImage = Renderer::GetRenderFrame();
		s_Data->QuadUniformBuffer->Update(&s_Data->camera->GetViewProjectMatrix(), currentImage, (uint8_t)sizeof(UniformBufferObject));		
		Renderer2D::End();

		vkCmdEndRenderPass(s_Data->CommandBuffer->GetActiveCommandBuffer());
		s_Data->CommandBuffer->End();
		s_Data->CommandBuffer->Submit();

	}

	void Renderer2D::End()
	{
		s_Data->QuadPipeline->Bind(s_Data->CommandBuffer->GetActiveCommandBuffer());
		s_Data->QuadVertexBuffer->Bind(s_Data->CommandBuffer->GetActiveCommandBuffer());
		s_Data->QuadIndexBuffer->Bind(s_Data->CommandBuffer->GetActiveCommandBuffer());
		s_Data->QuadPipeline->BindDescriptorSets(s_Data->CommandBuffer->GetActiveCommandBuffer());

		uint32_t dataSize = (uint8_t*)s_Data->QuadVertexBufferPtr - (uint8_t*)s_Data->QuadVertexBufferBase;
		s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase, dataSize);

		vkCmdDrawIndexed(s_Data->CommandBuffer->GetActiveCommandBuffer(), static_cast<uint32_t>(s_Data->QuadIndexCount), 1, 0, 0, 0);
		
		s_Data->QuadIndexCount = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;

		if (s_Data->QuadIndexCount >= Renderer2DData::MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			s_Data->QuadVertexBufferPtr->Color = color;
			s_Data->QuadVertexBufferPtr->texCoord = textureCoords[i];
			s_Data->QuadVertexBufferPtr->TexIndex = textureIndex;
			// s_Data->QuadVertexBufferPtr->TilingFactor = tilingFactor;
			s_Data->QuadVertexBufferPtr++;
		}

		s_Data->QuadIndexCount += 6;
	}
	
	void Renderer2D::FlushAndReset()
	{
		End();

		s_Data->QuadIndexCount = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase;
	}
	
	VkImageView Renderer2D::GetCompositeImageView()
	{
		return s_Data->RenderPass->GetImage().GetVulkanImageInfo().ImageView;
	}
}
