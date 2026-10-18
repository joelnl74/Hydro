#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"
#include "Hydro/Platform/Vulkan/VulkanRenderPass.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

// SHOULD BE IN IMGUI EXTENSION.
#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>
// SHOULD BE A IMGUI EXTENSION END BLOCK.

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Core/FileWatcher.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <chrono>

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

	// Everything one RenderView needs: its target, its camera and its quads.
	struct RenderViewData
	{
		std::string DebugName;

		// ---- Target ------------------------------------------------------------------
		Scope<VulkanRenderPass> Target;
		// Requested size (RenderView::SetSize). Applied at the start of the view's next render.
		uint32_t RequestedWidth = 0;
		uint32_t RequestedHeight = 0;
		// ImGui texture handles for the image of each frame in flight. Created lazily (ImGui may not be
		// initialised when the view is) and rewritten on resize.
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ImGuiTextures{};

		// ---- Camera ------------------------------------------------------------------
		glm::mat4 ViewProjection{ 1.0f };
		Ref<VulkanUniformBuffer> UniformBuffer;     // one buffer per frame in flight
		Ref<VulkanDescriptorBuilder> DescriptorSet; // set 0: camera + textures, laid out like the pipeline's

		// ---- Batches -----------------------------------------------------------------
		// Index i in each vector belongs to batch i. Batches are never freed, only
		// reused, so after the first heavy frame there are no more allocations.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameVertexBuffers> QuadVertexBuffers;  // GPU,         [batch][frame]
		std::vector<uint32_t> QuadIndexCounts;                 // indices used, [batch]

		uint32_t BatchIndex = 0;                     // batch currently being filled
		QuadVertex* QuadVertexBufferPtr = nullptr;   // write cursor into current batch

		bool Queued = false; // begun this frame, so Render() records it
	};

	struct Renderer2DData
	{
		// Quads per batch (= per draw call). 20k quads * 4 verts * 40 bytes = ~3.2 MB per buffer.
		// When a frame needs more, extra batches are allocated on demand.
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;
		static const uint32_t MaxTextureSlots = 32; // TODO QUERY FROM GPU

		glm::vec4 QuadVertexPositions[4];

		Ref<VulkanShader> Shader;
		// Shader hot reload: the shader's source files, checked every ShaderPollInterval.
		FileWatcher ShaderWatcher;
		std::chrono::steady_clock::time_point NextShaderPoll{};
		static constexpr std::chrono::milliseconds ShaderPollInterval{ 250 };

		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;      // shared by all batches of all views (every batch starts at vertex 0)

		// The pipeline is built against these. Every view's target uses the same attachment format,
		// so its render pass is compatible with PipelineRenderPass, and its descriptor set is built
		// exactly like LayoutDescriptorSet (BuildViewDescriptorSet), so it can be bound in its place.
		Scope<VulkanRenderPass> PipelineRenderPass;  // 1x1, never drawn into
		Ref<VulkanUniformBuffer> LayoutUniformBuffer;
		Ref<VulkanDescriptorBuilder> LayoutDescriptorSet;

		std::array<Ref<VullkanTexture>, MaxTextureSlots> TextureSlots;

		// Views begun this frame, in BeginScene order, and the one DrawQuad writes to.
		std::vector<RenderViewData*> QueuedViews;
		RenderViewData* CurrentView = nullptr;

		// GPU timestamps: 2 queries (begin/end of all 2D passes) per frame in flight.
		VkQueryPool TimestampPool = VK_NULL_HANDLE;
		float TimestampPeriodNs = 0.0f;  // ns per timestamp tick
		std::array<bool, MAX_FRAMES_IN_FLIGHT> TimestampsWritten{};
		float LastGPUTimeMs = -1.0f;
		char GPUName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};

		Statistics Stats;          // being accumulated this frame
		Statistics LastFrameStats; // what GetStats() returns
	};

	static Renderer2DData* s_Data = nullptr;

	// Set 0 of the quad shader: binding 0 = camera (vertex), binding 1 = texture array (fragment).
	static Ref<VulkanDescriptorBuilder> BuildViewDescriptorSet(Ref<VulkanUniformBuffer>& uniformBuffer)
	{
		Ref<VulkanDescriptorBuilder> descriptorSet = CreateRef<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindBuffer(0, uniformBuffer, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		descriptorSet->BindImage(1, s_Data->TextureSlots[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->Build();
		return descriptorSet;
	}

	void Renderer2D::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Renderer2D::Init called twice");
		s_Data = new Renderer2DData();

		// Created first: the pipeline is built against this render pass (see PipelineRenderPass).
		s_Data->PipelineRenderPass = CreateScope<VulkanRenderPass>(1, 1);

		/// Should be done before even entering the renderer shaders should have an get method from some sort of library.
		ShaderInformation vertexInformation;
		vertexInformation.path = "assets/shaders/vertex.vert";
		vertexInformation.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexInformation.shaderType = ShaderType::Vertex;

		ShaderInformation fragmentInformation;
		fragmentInformation.path = "assets/shaders/fragment.frag";
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
		s_Data->ShaderWatcher = FileWatcher(s_Data->Shader->GetSourceFiles());
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

		s_Data->LayoutUniformBuffer = CreateRef<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));
		s_Data->LayoutDescriptorSet = BuildViewDescriptorSet(s_Data->LayoutUniformBuffer);

		PipelineSpecification specification;
		specification.Layout =
		{
			{ "inPosition", ShaderDataType::Float3,},
			{ "inColor", ShaderDataType::Float4, },
			{ "inTexCoord", ShaderDataType::Float2, },
			{ "inTexIndex", ShaderDataType::Float, },
		};

		specification.DescriptorSets = { s_Data->LayoutDescriptorSet };
		specification.shader = s_Data->Shader;
		specification.RenderPass = s_Data->PipelineRenderPass->GetRenderPass();
		// Sprites: alpha blended, drawn in submission order, visible from both sides (negative scale flips them).
		specification.Blend = BlendMode::Alpha;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;

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

	void Renderer2D::Shutdown()
	{
		if (s_Data == nullptr)
			return;

		// Views belong to the application and must be gone by now (layers release them in OnDetach).
		HY_CORE_ASSERT(s_Data->QueuedViews.empty(), "A RenderView is still queued at Renderer2D::Shutdown");

		for (size_t i = 0; i < s_Data->TextureSlots.size(); i++)
		{
			if (s_Data->TextureSlots[i])
			{
				s_Data->TextureSlots[i]->Destroy();
			}
		}

		s_Data->LayoutDescriptorSet->Destroy();
		s_Data->QuadPipeline->Destroy();
		s_Data->LayoutUniformBuffer->Destroy();
		s_Data->QuadIndexBuffer->Destroy();
		s_Data->PipelineRenderPass.reset();
		if (s_Data->TimestampPool)
			vkDestroyQueryPool(Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice(), s_Data->TimestampPool, nullptr);

		delete s_Data;
		s_Data = nullptr;
	}

	// ---- RenderView -------------------------------------------------------------

	RenderView::RenderView(const RenderViewSpecification& specification)
		: m_Specification(specification), m_Data(CreateScope<RenderViewData>())
	{
		HY_CORE_ASSERT(s_Data != nullptr, "Create RenderViews after Renderer::Init");

		const uint32_t width = specification.Width > 0 ? specification.Width : 1;
		const uint32_t height = specification.Height > 0 ? specification.Height : 1;

		RenderViewData& view = *m_Data;
		view.DebugName = specification.DebugName;
		view.Target = CreateScope<VulkanRenderPass>(width, height);
		view.RequestedWidth = width;
		view.RequestedHeight = height;

		view.UniformBuffer = CreateRef<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));
		view.DescriptorSet = BuildViewDescriptorSet(view.UniformBuffer);

		// First batch. More are created by NextBatch() when a frame needs them.
		Renderer2D::AllocateBatch(view);
		Renderer2D::ResetBatches(view);
	}

	RenderView::~RenderView()
	{
		RenderViewData& view = *m_Data;

		// Never leave a dangling pointer in this frame's queue.
		if (s_Data != nullptr)
		{
			auto& queued = s_Data->QueuedViews;
			queued.erase(std::remove(queued.begin(), queued.end(), &view), queued.end());
			if (s_Data->CurrentView == &view)
				s_Data->CurrentView = nullptr;
		}

		for (auto& perFrame : view.QuadVertexBuffers)
			for (auto& vertexBuffer : perFrame)
				vertexBuffer->Destroy();

		for (QuadVertex* base : view.QuadVertexBufferBases)
			delete[] base;

		view.DescriptorSet->Destroy();
		view.UniformBuffer->Destroy();
		view.Target.reset();

		// The ImGui texture handles belong to ImGui's descriptor pool, which the ImGui layer frees
		// (this ImGui version has no RemoveTexture).
	}

	void RenderView::SetSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_Data->RequestedWidth = width;
		m_Data->RequestedHeight = height;
	}

	uint32_t RenderView::GetWidth() const
	{
		return m_Data->RequestedWidth;
	}

	uint32_t RenderView::GetHeight() const
	{
		return m_Data->RequestedHeight;
	}

	VkDescriptorSet RenderView::GetImGuiTexture()
	{
		const uint32_t frame = Renderer::GetRenderFrame();
		VkDescriptorSet& set = m_Data->ImGuiTextures[frame];

		if (set == VK_NULL_HANDLE)
		{
			const VulkanImageInfo& image = m_Data->Target->GetImage(frame).GetVulkanImageInfo();
			set = ImGui_ImplVulkan_AddTexture(image.Sampler, image.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		return set;
	}

	// ---- Batching ---------------------------------------------------------------

	void Renderer2D::AllocateBatch(RenderViewData& view)
	{
		view.QuadVertexBufferBases.push_back(new QuadVertex[Renderer2DData::MaxVertices]);

		PerFrameVertexBuffers buffers;
		for (auto& vertexBuffer : buffers)
			vertexBuffer = CreateRef<VulkanVertexBuffer>(Renderer2DData::MaxVertices * (uint32_t)sizeof(QuadVertex));
		view.QuadVertexBuffers.push_back(buffers);

		view.QuadIndexCounts.push_back(0);
	}

	void Renderer2D::NextBatch(RenderViewData& view)
	{
		// Current batch is full: move on to the next one, creating it the first time we need it.
		view.BatchIndex++;
		if (view.BatchIndex >= view.QuadVertexBufferBases.size())
			AllocateBatch(view);

		view.QuadIndexCounts[view.BatchIndex] = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[view.BatchIndex];
	}

	void Renderer2D::ResetBatches(RenderViewData& view)
	{
		for (uint32_t& count : view.QuadIndexCounts)
			count = 0;

		view.BatchIndex = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[0];
	}

	// ---- Scenes -----------------------------------------------------------------

	void Renderer2D::BeginScene(RenderView& view, const glm::mat4& viewProjection)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::BeginScene without EndScene");

		RenderViewData& data = *view.m_Data;
		data.ViewProjection = viewProjection;

		if (data.Queued == false)
		{
			data.Queued = true;
			s_Data->QueuedViews.push_back(&data);
		}

		s_Data->CurrentView = &data;
	}

	void Renderer2D::BeginScene(RenderView& view, const Camera& camera, const glm::mat4& cameraTransform)
	{
		BeginScene(view, camera.GetProjection() * glm::inverse(cameraTransform));
	}

	void Renderer2D::EndScene()
	{
		s_Data->CurrentView = nullptr;
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawQuad outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
			return;

		RenderViewData& view = *s_Data->CurrentView;

		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		// Batch full? Start a new one instead of overflowing (or flushing mid-frame).
		if (view.QuadIndexCounts[view.BatchIndex] >= Renderer2DData::MaxIndices)
			NextBatch(view);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			view.QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			view.QuadVertexBufferPtr->Color = color;
			view.QuadVertexBufferPtr->texCoord = textureCoords[i];
			view.QuadVertexBufferPtr->TexIndex = textureIndex;
			view.QuadVertexBufferPtr++;
		}

		view.QuadIndexCounts[view.BatchIndex] += 6;
		s_Data->Stats.QuadCount++;
	}

	// ---- Frame ------------------------------------------------------------------

	void Renderer2D::Render(VkCommandBuffer commandBuffer)
	{
		HY_CORE_ASSERT(s_Data->CurrentView == nullptr, "Renderer2D::Render inside BeginScene/EndScene");
		s_Data->CurrentView = nullptr;

		ApplySettings();

		const uint32_t frame = Renderer::GetRenderFrame();

		// Renderer::RenderFrame already waited on this frame-in-flight's fence, so the GPU is done
		// with every view's buffers for `frame`; safe to overwrite.

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

		for (RenderViewData* view : s_Data->QueuedViews)
		{
			RecordView(commandBuffer, *view, frame);
			s_Data->Stats.ViewCount++;
		}

		if (s_Data->TimestampPool)
		{
			vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery + 1);
			s_Data->TimestampsWritten[frame] = true;
		}

		// Frame-level info for the stats panel.
		GPUMemoryStats memory = VulkanAllocator::GetStats();
		s_Data->Stats.GPUMemoryUsed = memory.Used;
		s_Data->Stats.GPUMemoryFree = memory.Free;
		s_Data->Stats.MaxQuadsPerBatch = Renderer2DData::MaxQuads;
		s_Data->Stats.FramesInFlight = MAX_FRAMES_IN_FLIGHT;
		s_Data->Stats.FrameIndex = frame;
		s_Data->Stats.GPUTimeMs = s_Data->LastGPUTimeMs;
		s_Data->Stats.GPUName = s_Data->GPUName;
		if (s_Data->QueuedViews.empty() == false)
		{
			s_Data->Stats.ViewportWidth = s_Data->QueuedViews.front()->Target->GetWidth();
			s_Data->Stats.ViewportHeight = s_Data->QueuedViews.front()->Target->GetHeight();
		}

		s_Data->LastFrameStats = s_Data->Stats;
		s_Data->Stats = {};
		DiscardFrame();
	}

	void Renderer2D::RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		ApplyViewResize(view);

		const uint32_t width = view.Target->GetWidth();
		const uint32_t height = view.Target->GetHeight();

		UniformBufferObject ubo;
		ubo.mvp = view.ViewProjection;
		view.UniformBuffer->Update(&ubo, frame, (uint32_t)sizeof(UniformBufferObject));

		// One render pass per view per frame => its target is cleared exactly once. Its subpass
		// dependencies make the result visible to ImGui, which samples it later in this command buffer.
		view.Target->Begin(commandBuffer, frame, Renderer::GetSettings().ClearColor);

		// State shared by every batch: bind once. Viewport/scissor follow the view's size.
		s_Data->QuadPipeline->Bind(commandBuffer, { width, height });
		s_Data->QuadIndexBuffer->Bind(commandBuffer);
		s_Data->QuadPipeline->BindDescriptorSet(commandBuffer, 0, *view.DescriptorSet);

		// One draw call per batch. Each batch has its own vertex buffer, so uploading
		// batch i+1 never overwrites vertices that batch i's draw still has to read.
		for (uint32_t i = 0; i <= view.BatchIndex; i++)
		{
			const uint32_t indexCount = view.QuadIndexCounts[i];
			if (indexCount == 0)
				continue;

			const uint32_t vertexCount = (indexCount / 6) * 4;
			auto& vertexBuffer = view.QuadVertexBuffers[i][frame];

			vertexBuffer->SetData(view.QuadVertexBufferBases[i], vertexCount * (uint32_t)sizeof(QuadVertex));
			vertexBuffer->Bind(commandBuffer);

			vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
			s_Data->Stats.DrawCalls++;
			s_Data->Stats.BatchCount++;
			s_Data->Stats.VertexBytesUploaded += (uint64_t)vertexCount * sizeof(QuadVertex);
		}

		s_Data->Stats.BatchesAllocated += (uint32_t)view.QuadVertexBufferBases.size();

		view.Target->End(commandBuffer);
	}

	void Renderer2D::DiscardFrame()
	{
		for (RenderViewData* view : s_Data->QueuedViews)
		{
			view->Queued = false;
			ResetBatches(*view);
		}

		s_Data->QueuedViews.clear();
		s_Data->CurrentView = nullptr;
		s_Data->Stats = {};
	}

	Statistics Renderer2D::GetStats()
	{
		return s_Data->LastFrameStats;
	}

	static void WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler)
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

	void Renderer2D::ApplyViewResize(RenderViewData& view)
	{
		const uint32_t width = view.RequestedWidth;
		const uint32_t height = view.RequestedHeight;

		if (width == view.Target->GetWidth() && height == view.Target->GetHeight())
			return;

		// Both frames in flight may still be using the old images; wait before destroying them.
		// (Only happens on frames where the size actually changed.)
		Renderer::WaitIdle();

		view.Target->Resize(width, height);

		// Point the existing ImGui texture handles at the new image views. Updating in place
		// (instead of AddTexture again) means no descriptor sets leak from ImGui's pool, and the
		// handle ImGui already queued for this frame shows the new image.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (view.ImGuiTextures[i] != VK_NULL_HANDLE)
			{
				const VulkanImageInfo& image = view.Target->GetImage(i).GetVulkanImageInfo();
				WriteImGuiTexture(view.ImGuiTextures[i], image.ImageView, image.Sampler);
			}
		}
	}

	void Renderer2D::ApplySettings()
	{
		const RendererSettings& settings = Renderer::GetSettings();

		// The wireframe flag is baked into the pipeline, so a change means a rebuild.
		// Only rebuild when the GPU can do it; otherwise VulkanPipeline would warn every frame.
		const bool wireframe = settings.Wireframe && Renderer::GetCapabilities().Wireframe;
		if (wireframe != s_Data->QuadPipeline->GetSpecification().Wireframe)
			RebuildQuadPipeline(false, wireframe);

		ReloadShadersIfChanged();
	}

	void Renderer2D::ReloadShadersIfChanged()
	{
		if (Renderer::GetSettings().ShaderHotReload == false)
			return;

		// A few checks per second is instant enough and keeps file-system calls off most frames.
		const auto now = std::chrono::steady_clock::now();
		if (now < s_Data->NextShaderPoll)
			return;

		s_Data->NextShaderPoll = now + Renderer2DData::ShaderPollInterval;
		if (s_Data->ShaderWatcher.Poll() == false)
			return;

		// Poll() has already taken the new file times, so a broken shader is retried on the next save,
		// not every poll.
		if (RebuildQuadPipeline(true, s_Data->QuadPipeline->GetSpecification().Wireframe))
			HY_CORE_INFO("Shaders reloaded.");
	}

	bool Renderer2D::RebuildQuadPipeline(bool recompileShader, bool wireframe)
	{
		// Build the replacements first, so a shader that doesn't compile leaves the old one drawing.
		Ref<VulkanShader> shader = s_Data->Shader;
		Ref<VulkanPipeline> pipeline;
		try
		{
			if (recompileShader)
				shader = CreateRef<VulkanShader>(s_Data->Shader->GetSpecification());

			PipelineSpecification specification = s_Data->QuadPipeline->GetSpecification();
			specification.shader = shader;
			specification.Wireframe = wireframe;
			pipeline = CreateRef<VulkanPipeline>(specification);
		}
		catch (const std::exception&)
		{
			// VulkanShader already logged the compiler output.
			HY_CORE_WARN("Quad pipeline rebuild failed; still using the previous one.");
			return false;
		}

		// Both frames in flight may still be using the old pipeline (same as ApplyViewResize).
		Renderer::WaitIdle();

		s_Data->QuadPipeline->Destroy();
		s_Data->QuadPipeline = pipeline;
		if (recompileShader)
		{
			s_Data->Shader = shader;
			s_Data->ShaderWatcher = FileWatcher(shader->GetSourceFiles());
		}

		return true;
	}
}
