#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"
#include "Hydro/Platform/Vulkan/VulkanRenderPass.h"

// SHOULD BE IN IMGUI EXTENSION.
#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>
// SHOULD BE A IMGUI EXTENSION END BLOCK.

#include "Hydro/Renderer/Renderer.h"

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

		Scope<VulkanRenderPass> RenderPass;

		// Requested offscreen size (from SetViewportSize). Applied in Render().
		uint32_t RequestedWidth = 0;
		uint32_t RequestedHeight = 0;

		std::array<Ref<VullkanTexture>, MaxTextureSlots> TextureSlots;

		// GPU timestamps: 2 queries (begin/end of the pass) per frame in flight.
		VkQueryPool TimestampPool = VK_NULL_HANDLE;
		float TimestampPeriodNs = 0.0f;  // ns per timestamp tick
		std::array<bool, MAX_FRAMES_IN_FLIGHT> TimestampsWritten{};
		float LastGPUTimeMs = -1.0f;
		char GPUName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};

		Statistics Stats;          // being accumulated this frame
		Statistics LastFrameStats; // what GetStats() returns

		// ImGui texture handles for the offscreen image of each frame in flight.
		// Created lazily (ImGui isn't initialised yet in Init) and rewritten on resize.
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> CompositeDescriptorSets{};
	};

	static Renderer2DData* s_Data = nullptr;

	void Renderer2D::Init(uint32_t width, uint32_t height)
	{
		s_Data = new Renderer2DData();
		s_Data->camera = CreateRef<Camera>(0, (float)width, 0, (float)height);

		// Offscreen target. Created first: the pipeline is built against this render pass.
		// It starts at the window size; the editor resizes it to the viewport panel (SetViewportSize).
		s_Data->RenderPass = CreateScope<VulkanRenderPass>(width, height);
		s_Data->RequestedWidth = width;
		s_Data->RequestedHeight = height;

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
			{ "inPosition", ShaderDataType::Float3,},
			{ "inColor", ShaderDataType::Float4, },
			{ "inTexCoord", ShaderDataType::Float2, },
			{ "inTexIndex", ShaderDataType::Float, },
		};

		specification.descriptorSet = s_Data->QuadDescriptorSet;
		specification.shader = s_Data->Shader;
		specification.RenderPass = s_Data->RenderPass->GetRenderPass();

		s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);

		// GPU timing.
		{
			auto device = Renderer::GetRendererContext()->GetVulkanDevice();
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(device->GetPhysicalDevice()->GetVulkanPhysicalDevice(), &properties);
			memcpy(s_Data->GPUName, properties.deviceName, sizeof(s_Data->GPUName));

			if (properties.limits.timestampComputeAndGraphics)
			{
				s_Data->TimestampPeriodNs = properties.limits.timestampPeriod;

				VkQueryPoolCreateInfo queryPoolInfo{};
				queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
				queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
				queryPoolInfo.queryCount = 2 * MAX_FRAMES_IN_FLIGHT;
				vkCreateQueryPool(device->GetDevice(), &queryPoolInfo, nullptr, &s_Data->TimestampPool);
			}
		}
	}

	void Renderer2D::ShutDown()
	{
		if (s_Data == nullptr)
			return;

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
		s_Data->RenderPass.reset();
		if (s_Data->TimestampPool)
			vkDestroyQueryPool(Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice(), s_Data->TimestampPool, nullptr);

		// The composite descriptor sets belong to ImGui's descriptor pool, which the ImGui layer frees.
		// ~VulkanUniformBuffer is now safe after Destroy(), so the data can be freed.
		delete s_Data;
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

	void Renderer2D::Render(VkCommandBuffer commandBuffer)
	{
		ApplyViewportResize();

		const uint32_t frame = Renderer::GetRenderFrame();
		const uint32_t width = s_Data->RenderPass->GetWidth();
		const uint32_t height = s_Data->RenderPass->GetHeight();

		// Renderer::RenderFrame already waited on this frame-in-flight's fence, so the GPU is done
		// with QuadVertexBuffers[*][frame] and the uniform buffer for `frame`; safe to overwrite.

		// The fence for this frame index was just waited on, so the timestamps this command
		// buffer wrote last time are available: read them, then reset and write new ones.
		const uint32_t firstQuery = frame * 2;
		if (s_Data->TimestampPool)
		{
			if (s_Data->TimestampsWritten[frame])
			{
				uint64_t timestamps[2] = {};
				VkResult result = vkGetQueryPoolResults(Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice(),
					s_Data->TimestampPool, firstQuery, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
				if (result == VK_SUCCESS && timestamps[1] >= timestamps[0])
					s_Data->LastGPUTimeMs = (float)((double)(timestamps[1] - timestamps[0]) * s_Data->TimestampPeriodNs / 1.0e6);
			}

			vkCmdResetQueryPool(commandBuffer, s_Data->TimestampPool, firstQuery, 2);
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery);
		}

		s_Data->QuadUniformBuffer->Update(&s_Data->camera->GetViewProjectMatrix(), frame, (uint32_t)sizeof(UniformBufferObject));

		// One render pass per frame => the target is cleared exactly once. Its subpass dependencies
		// make the result visible to the ImGui viewport, which samples it later in this command buffer.
		s_Data->RenderPass->Begin(commandBuffer, frame);

		// State shared by every batch: bind once. Viewport/scissor follow the current target size.
		s_Data->QuadPipeline->Bind(commandBuffer, { width, height });

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

		s_Data->RenderPass->End(commandBuffer);

		if (s_Data->TimestampPool)
		{
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery + 1);
			s_Data->TimestampsWritten[frame] = true;
		}

		// Frame-level info for the stats panel.
		GPUMemoryStats memory = VulkanAllocator::GetStats();
		s_Data->Stats.GPUMemoryUsed = memory.Used;
		s_Data->Stats.GPUMemoryFree = memory.Free;
		s_Data->Stats.BatchesAllocated = (uint32_t)s_Data->QuadVertexBufferBases.size();
		s_Data->Stats.MaxQuadsPerBatch = Renderer2DData::MaxQuads;
		s_Data->Stats.ViewportWidth = width;
		s_Data->Stats.ViewportHeight = height;
		s_Data->Stats.FramesInFlight = MAX_FRAMES_IN_FLIGHT;
		s_Data->Stats.FrameIndex = frame;
		s_Data->Stats.GPUTimeMs = s_Data->LastGPUTimeMs;
		s_Data->Stats.GPUName = s_Data->GPUName;

		s_Data->LastFrameStats = s_Data->Stats;
		s_Data->Stats = {};
		ResetBatches();
	}

	void Renderer2D::DiscardFrame()
	{
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

	static void WriteCompositeDescriptor(VkDescriptorSet set, VkImageView view, VkSampler sampler)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = sampler;
		imageInfo.imageView = view;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Binding 0 / combined image sampler is the layout ImGui_ImplVulkan_AddTexture uses.
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void Renderer2D::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		s_Data->RequestedWidth = width;
		s_Data->RequestedHeight = height;
	}

	void Renderer2D::ApplyViewportResize()
	{
		const uint32_t width = s_Data->RequestedWidth;
		const uint32_t height = s_Data->RequestedHeight;

		if (width == s_Data->RenderPass->GetWidth() && height == s_Data->RenderPass->GetHeight())
			return;

		// Both frames in flight may still be using the old images; wait before destroying them.
		// (Only happens on frames where the size actually changed.)
		vkDeviceWaitIdle(Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice());

		s_Data->RenderPass->Resize(width, height);

		// Keep 1 unit = 1 pixel, so things don't stretch when the viewport changes shape.
		s_Data->camera->SetProjection(0.0f, (float)width, 0.0f, (float)height);

		// Point the existing ImGui texture handles at the new image views. Updating in place
		// (instead of AddTexture again) means no descriptor sets leak from ImGui's pool, and the
		// handle ImGui already queued for this frame shows the new image.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (s_Data->CompositeDescriptorSets[i] != VK_NULL_HANDLE)
			{
				const VulkanImageInfo& image = s_Data->RenderPass->GetImage(i).GetVulkanImageInfo();
				WriteCompositeDescriptor(s_Data->CompositeDescriptorSets[i], image.ImageView, image.Sampler);
			}
		}
	}

	VkDescriptorSet Renderer2D::GetCompositeDescriptorSet()
	{
		const uint32_t frame = Renderer::GetRenderFrame();
		VkDescriptorSet& set = s_Data->CompositeDescriptorSets[frame];

		if (set == VK_NULL_HANDLE)
		{
			const VulkanImageInfo& image = s_Data->RenderPass->GetImage(frame).GetVulkanImageInfo();
			set = ImGui_ImplVulkan_AddTexture(image.Sampler, image.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		return set;
	}
}
