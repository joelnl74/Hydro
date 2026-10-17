#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"
#include "Hydro/Platform/Vulkan/VulkanRenderCommandBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanRenderPass.h"

// SHOULD BE IN IMGUI EXTENSION.
#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>
// SHOULD BE A IMGUI EXTENSION END BLOCK.

#include "Hydro/Renderer/Renderer.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Renderer/Camera.h"

#include <array>

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

	// One GPU vertex buffer per frame in flight. While frame N is still being read by
	// the GPU, frame N+1 writes into its own buffer, so the CPU never overwrites data
	// the GPU is using.
	using PerFrameVertexBuffers = std::array<Ref<VulkanVertexBuffer>, MAX_FRAMES_IN_FLIGHT>;

	struct Renderer2DData
	{
		// Quads per batch (= per draw call). 20k quads * 4 verts * 40 bytes = ~3.2 MB per buffer.
		// When a frame needs more, extra batches are allocated on demand.
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO QUERY FROM GPU

		// ---- Batches -------------------------------------------------------------
		// Index i in each vector belongs to batch i. Batches are never freed, only
		// reused, so after the first heavy frame there are no more allocations.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameVertexBuffers> QuadVertexBuffers;  // GPU,         [batch][frame]
		std::vector<uint32_t> QuadIndexCounts;                 // indices used, [batch]

		uint32_t BatchIndex = 0;                     // batch currently being filled
		QuadVertex* QuadVertexBufferPtr = nullptr;   // write cursor into current batch
		// ---------------------------------------------------------------------------

		glm::vec4 QuadVertexPositions[4];

		Ref<Camera> camera;
		UniformBufferObject ubo;

		Ref<VulkanShader> Shader;

		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;      // shared by all batches (every batch starts at vertex 0)
		Ref<VulkanUniformBuffer> QuadUniformBuffer;
		Ref<VulkanDescriptorBuilder> QuadDescriptorSet;

		Ref<VulkanRenderCommandBuffer> CommandBuffer;
		Ref<VulkanRenderPass> RenderPass;

		std::array<Ref<VullkanTexture>, MaxTextureSlots> TextureSlots;

		Statistics Stats;          // being accumulated this frame
		Statistics LastFrameStats; // what GetStats() returns

		// THIS IS A HACK! We need to store the descriptor sets for the pipeline.
		std::vector <VkDescriptorSet> m_DescriptorSets;
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

		// Counter-clockwise starting bottom-left, matching the texture coords in DrawQuad.
		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data->Shader = CreateRef<VulkanShader>(shaderSpecification);
		/// END BLOCK!

		VulkanTextureSpecification textureProperties;

		s_Data->TextureSlots[0] = CreateRef<VullkanTexture>("assets/textures/white.png", textureProperties);

		// Index buffer. FIX: indices are uint32_t (and bound as VK_INDEX_TYPE_UINT32), so the
		// buffer must be MaxIndices * 4 bytes. It used to be sized with sizeof(uint16_t), so only
		// the first half was uploaded and any draw past MaxQuads/2 read outside the buffer.
		uint32_t* quadIndices = new uint32_t[Renderer2DData::MaxIndices];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < Renderer2DData::MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>(quadIndices, Renderer2DData::MaxIndices * (uint32_t)sizeof(uint32_t));
		delete[] quadIndices;

		// First batch. More are created by NextBatch() when a frame needs them.
		AllocateBatch();
		ResetBatches();

		s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));

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

		// Command Buffer and FrameBuffer render target for renderer 2D.
		s_Data->CommandBuffer = CreateRef<VulkanRenderCommandBuffer>(MAX_FRAMES_IN_FLIGHT, "2DRenderer");
		s_Data->RenderPass = CreateRef<VulkanRenderPass>();
	}

	void Renderer2D::ShutDown()
	{
		for (size_t i = 0; i < s_Data->TextureSlots.size(); i++)
		{
			if (s_Data->TextureSlots[i])
			{
				s_Data->TextureSlots[i]->Destroy();
			}
		}

		for (auto& perFrame : s_Data->QuadVertexBuffers)
			for (auto& vertexBuffer : perFrame)
				vertexBuffer->Destroy();

		for (QuadVertex* base : s_Data->QuadVertexBufferBases)
			delete[] base;

		s_Data->QuadDescriptorSet->Destroy();
		s_Data->Shader->Destroy();
		s_Data->QuadPipeline->Destroy();
		s_Data->QuadUniformBuffer->Destroy();
		s_Data->QuadIndexBuffer->Destroy();

		// NOTE: s_Data is intentionally not deleted yet: ~VulkanUniformBuffer calls
		// vkDestroyBuffer on buffers Destroy() already freed (double free). Fix that
		// destructor first, then `delete s_Data;` here.
		s_Data = nullptr;
	}

	// ---- Batching ---------------------------------------------------------------

	void Renderer2D::AllocateBatch()
	{
		s_Data->QuadVertexBufferBases.push_back(new QuadVertex[Renderer2DData::MaxVertices]);

		PerFrameVertexBuffers buffers;
		for (auto& vertexBuffer : buffers)
			vertexBuffer = CreateRef<VulkanVertexBuffer>(Renderer2DData::MaxVertices * (uint32_t)sizeof(QuadVertex));
		s_Data->QuadVertexBuffers.push_back(buffers);

		s_Data->QuadIndexCounts.push_back(0);
	}

	void Renderer2D::NextBatch()
	{
		// Current batch is full: move on to the next one, creating it the first time we need it.
		s_Data->BatchIndex++;
		if (s_Data->BatchIndex >= s_Data->QuadVertexBufferBases.size())
			AllocateBatch();

		s_Data->QuadIndexCounts[s_Data->BatchIndex] = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBases[s_Data->BatchIndex];
	}

	void Renderer2D::ResetBatches()
	{
		for (uint32_t& count : s_Data->QuadIndexCounts)
			count = 0;

		s_Data->BatchIndex = 0;
		s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBases[0];
	}

	// ---- Frame ------------------------------------------------------------------

	void Renderer2D::Render()
	{
		auto& extent = Renderer::GetVulkanSwapChain()->GetExtend();
		const uint32_t frame = Renderer::GetRenderFrame();

		// Waits on this frame-in-flight's fence, so after this the GPU is done with
		// QuadVertexBuffers[*][frame] and the uniform buffer for `frame`; safe to overwrite.
		s_Data->CommandBuffer->Begin();
		VkCommandBuffer commandBuffer = s_Data->CommandBuffer->GetActiveCommandBuffer();

		s_Data->QuadUniformBuffer->Update(&s_Data->camera->GetViewProjectMatrix(), frame, (uint32_t)sizeof(UniformBufferObject));

		// One render pass per frame => the target is cleared exactly once.
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = s_Data->RenderPass->GetRenderPass();
		renderPassInfo.framebuffer = s_Data->RenderPass->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// State shared by every batch: bind once.
		s_Data->QuadPipeline->Bind(commandBuffer);
		s_Data->QuadIndexBuffer->Bind(commandBuffer);
		s_Data->QuadPipeline->BindDescriptorSets(commandBuffer);

		// One draw call per batch. Each batch has its own vertex buffer, so uploading
		// batch i+1 never overwrites vertices that batch i's draw still has to read.
		for (uint32_t i = 0; i <= s_Data->BatchIndex; i++)
		{
			const uint32_t indexCount = s_Data->QuadIndexCounts[i];
			if (indexCount == 0)
				continue;

			const uint32_t vertexCount = (indexCount / 6) * 4;
			auto& vertexBuffer = s_Data->QuadVertexBuffers[i][frame];

			vertexBuffer->SetData(s_Data->QuadVertexBufferBases[i], vertexCount * (uint32_t)sizeof(QuadVertex));
			vertexBuffer->Bind(commandBuffer);

			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
			s_Data->Stats.DrawCalls++;
			s_Data->Stats.BatchCount++;
			s_Data->Stats.VertexBytesUploaded += (uint64_t)vertexCount * sizeof(QuadVertex);
		}

		vkCmdEndRenderPass(commandBuffer);
		s_Data->CommandBuffer->End();
		s_Data->CommandBuffer->Submit();

		// Frame-level info for the stats panel.
		GPUMemoryStats memory = VulkanAllocator::GetStats();
		s_Data->Stats.GPUMemoryUsed = memory.Used;
		s_Data->Stats.GPUMemoryFree = memory.Free;
		s_Data->Stats.BatchesAllocated = (uint32_t)s_Data->QuadVertexBufferBases.size();
		s_Data->Stats.MaxQuadsPerBatch = Renderer2DData::MaxQuads;
		s_Data->Stats.ViewportWidth = extent.width;
		s_Data->Stats.ViewportHeight = extent.height;
		s_Data->Stats.FramesInFlight = MAX_FRAMES_IN_FLIGHT;
		s_Data->Stats.FrameIndex = frame;

		s_Data->LastFrameStats = s_Data->Stats;
		s_Data->Stats = {};
		ResetBatches();
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		// Batch full? Start a new one instead of overflowing (or flushing mid-frame).
		if (s_Data->QuadIndexCounts[s_Data->BatchIndex] >= Renderer2DData::MaxIndices)
			NextBatch();

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			s_Data->QuadVertexBufferPtr->Color = color;
			s_Data->QuadVertexBufferPtr->texCoord = textureCoords[i];
			s_Data->QuadVertexBufferPtr->TexIndex = textureIndex;
			s_Data->QuadVertexBufferPtr++;
		}

		s_Data->QuadIndexCounts[s_Data->BatchIndex] += 6;
		s_Data->Stats.QuadCount++;
	}

	Statistics Renderer2D::GetStats()
	{
		return s_Data->LastFrameStats;
	}

	void Renderer2D::CreateCompositeImageView()
	{
		uint32_t currentImage = Renderer::GetRenderFrame();
		auto sampler = Renderer::GetVulkanSwapChain()->GetImageSampler();

		if (s_Data->m_DescriptorSets.size() <= currentImage)
		{
			VkDescriptorSet x = ImGui_ImplVulkan_AddTexture(sampler, s_Data->RenderPass->GetImage().GetVulkanImageInfo().ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			s_Data->m_DescriptorSets.push_back(x);
		}
	}

	VkDescriptorSet Renderer2D::GetCompositeDescriptorSet()
	{
		CreateCompositeImageView();
		uint32_t currentImage = Renderer::GetRenderFrame();
		return s_Data->m_DescriptorSets[currentImage];
	}
}
