#include <hypch.h>

#include "Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanRenderPass.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

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
		glm::vec2 TexCoord;
		float TexIndex;
	};

	struct UniformBufferObject
	{
		alignas(16) glm::mat4 mvp;
	};

	// Most textures one draw call can sample. The slots actually used (Renderer2DData::TextureSlots)
	// are 32, or 16 on a GPU that allows fewer than 32 samplers per shader stage (Vulkan guarantees 16;
	// some Metal drivers on macOS stop there). The quad shader gets the same number as HY_TEXTURE_SLOTS
	// and sizes `inTextures[]` and its switch with it; the descriptor set's image array matches.
	static constexpr uint32_t MaxTextureSlots = VulkanDescriptorBuilder::MaxImageArraySize;
	static constexpr uint32_t MinTextureSlots = 16;
	static_assert(MaxTextureSlots == 32, "Change inTextures[] and SampleTexture in fragment.frag too");

	// One GPU vertex buffer per frame in flight. While frame N is still being read by
	// the GPU, frame N+1 writes into its own buffer, so the CPU never overwrites data
	// the GPU is using.
	using PerFrameVertexBuffers = std::array<Scope<VulkanVertexBuffer>, MAX_FRAMES_IN_FLIGHT>;

	// Up to MaxTextureSlots textures that one or more draws sample from, and the set 0 descriptor sets
	// (camera + texture array, one per frame in flight) that bind them. Slot 0 is always the white texture.
	struct TextureSet
	{
		std::array<Ref<Texture2D>, MaxTextureSlots> Slots;
		uint32_t SlotCount = 1;
		Scope<VulkanDescriptorBuilder> DescriptorSet;
	};

	// One vkCmdDrawIndexed: a run of quads in one vertex batch that use one texture set.
	struct QuadDraw
	{
		uint32_t Batch = 0;       // vertex batch the quads are in
		uint32_t FirstIndex = 0;  // first index in that batch (6 per quad)
		uint32_t IndexCount = 0;
		uint32_t TextureSet = 0;
	};

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
		// initialised when the view is). After a resize each one is pointed at its new image when its
		// frame index is recorded next (a frame still in flight may be drawing with the old one).
		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ImGuiTextures{};
		std::array<bool, MAX_FRAMES_IN_FLIGHT> ImGuiTexturesOutdated{};

		// ---- Camera ------------------------------------------------------------------
		glm::mat4 ViewProjection{ 1.0f };
		Scope<VulkanUniformBuffer> UniformBuffer;   // one buffer per frame in flight

		// ---- Batches -----------------------------------------------------------------
		// Vertex storage. Index i in each vector belongs to batch i. Batches are never freed, only
		// reused, so after the first heavy frame there are no more allocations.
		std::vector<QuadVertex*> QuadVertexBufferBases;        // CPU staging, [batch]
		std::vector<PerFrameVertexBuffers> QuadVertexBuffers;  // GPU,         [batch][frame]
		std::vector<uint32_t> QuadIndexCounts;                 // indices used, [batch]

		uint32_t BatchIndex = 0;                     // batch currently being filled
		QuadVertex* QuadVertexBufferPtr = nullptr;   // write cursor into current batch

		// ---- Textures and draws ------------------------------------------------------
		// Texture sets are reused like batches (their descriptor sets are created once).
		std::vector<TextureSet> TextureSets;
		uint32_t TextureSetIndex = 0;                // set currently being filled
		// This frame's draw calls in submission order. A new draw starts when the batch is full
		// or the texture set is full.
		std::vector<QuadDraw> Draws;

		bool Queued = false; // begun this frame, so Render() records it
	};

	struct Renderer2DData
	{
		// Quads per batch (= per vertex buffer). 20k quads * 4 verts * 40 bytes = ~3.2 MB per buffer.
		// When a frame needs more, extra batches are allocated on demand.
		static const uint32_t MaxQuads = 20000;
		static const uint32_t MaxVertices = MaxQuads * 4;
		static const uint32_t MaxIndices = MaxQuads * 6;

		glm::vec4 QuadVertexPositions[4];

		Scope<VulkanShader> Shader;
		// Texture slots per draw on this GPU: MaxTextureSlots or MinTextureSlots (see above).
		uint32_t TextureSlots = MaxTextureSlots;
		// Shader hot reload: the shader's source files, checked every ShaderPollInterval.
		FileWatcher ShaderWatcher;
		std::chrono::steady_clock::time_point NextShaderPoll{};
		static constexpr std::chrono::milliseconds ShaderPollInterval{ 250 };

		Scope<VulkanPipeline> QuadPipeline;
		Scope<VulkanIndexBuffer> QuadIndexBuffer;    // shared by all batches of all views (every batch starts at vertex 0)

		// The pipeline is built against these. Every view's target uses the same attachment format,
		// so its render pass is compatible with PipelineRenderPass, and its descriptor sets are built
		// exactly like LayoutDescriptorSet (BuildViewDescriptorSet), so they can be bound in its place.
		Scope<VulkanRenderPass> PipelineRenderPass;  // 1x1, never drawn into
		Scope<VulkanUniformBuffer> LayoutUniformBuffer;
		Scope<VulkanDescriptorBuilder> LayoutDescriptorSet;

		// Slot 0 of every texture set; made in code (no file).
		Ref<Texture2D> WhiteTexture;

		// Every RenderView that exists (they register themselves).
		std::vector<RenderViewData*> Views;

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

	static const VulkanImageInfo& GetImageInfo(const Texture2D& texture)
	{
		return GetTexture2DData(texture).Image->GetVulkanImageInfo();
	}

	// Set 0 of the quad shader: binding 0 = camera (vertex), binding 1 = texture array (fragment).
	// Every texture slot starts as the white texture; RecordView writes the real ones each frame.
	static Scope<VulkanDescriptorBuilder> BuildViewDescriptorSet(const VulkanUniformBuffer& uniformBuffer)
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);

		Scope<VulkanDescriptorBuilder> descriptorSet = CreateScope<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindBuffer(0, uniformBuffer, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		descriptorSet->BindImage(1, white.ImageView, white.Sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, s_Data->TextureSlots);
		descriptorSet->Build();
		return descriptorSet;
	}

	// Points the texture array of `set`'s descriptor set for `frame` at its textures (unused slots: white).
	static void WriteTextureSet(TextureSet& set, uint32_t frame)
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);

		const uint32_t slotCount = s_Data->TextureSlots;
		std::array<VkDescriptorImageInfo, MaxTextureSlots> images{};
		for (uint32_t i = 0; i < slotCount; i++)
		{
			const VulkanImageInfo& image = (i < set.SlotCount && set.Slots[i] != nullptr) ? GetImageInfo(*set.Slots[i]) : white;
			images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			images[i].imageView = image.ImageView;
			images[i].sampler = image.Sampler;
		}

		set.DescriptorSet->WriteImages(frame, 1, images.data(), slotCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	void Renderer2D::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Renderer2D::Init called twice");
		s_Data = new Renderer2DData();

		// How many textures one draw can sample on this GPU decides the shader and descriptor layout.
		{
			const VkPhysicalDeviceLimits& limits = Renderer::GetVulkanDevice().GetPhysicalDevice().GetProperties().limits;
			const uint32_t samplerLimit = std::min(limits.maxPerStageDescriptorSamplers, limits.maxPerStageDescriptorSampledImages);
			s_Data->TextureSlots = samplerLimit >= MaxTextureSlots ? MaxTextureSlots : MinTextureSlots;
			if (s_Data->TextureSlots < MaxTextureSlots)
			{
				HY_CORE_WARN("Renderer2D: the GPU allows {} textures per shader stage, so a draw uses {} texture slots instead of {}", samplerLimit, s_Data->TextureSlots, MaxTextureSlots);
			}
		}

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
		shaderSpecification.Defines["HY_TEXTURE_SLOTS"] = std::to_string(s_Data->TextureSlots);

		// Counter-clockwise starting bottom-left, matching the texture coords in SubmitQuad.
		s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
		s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		s_Data->Shader = CreateScope<VulkanShader>(shaderSpecification);
		s_Data->ShaderWatcher = FileWatcher(s_Data->Shader->GetSourceFiles());
		/// END BLOCK!

		// The white texture is made in code, so plain coloured quads need no file.
		{
			const uint32_t whitePixel = 0xffffffff;
			ImageSpecification whiteSpecification;
			whiteSpecification.Width = 1;
			whiteSpecification.Height = 1;
			whiteSpecification.Format = ImageFormat::SRGB;
			s_Data->WhiteTexture = CreateRef<Texture2D>(whiteSpecification, &whitePixel);
		}

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

		s_Data->QuadIndexBuffer = CreateScope<VulkanIndexBuffer>(quadIndices, Renderer2DData::MaxIndices * (uint32_t)sizeof(uint32_t));
		delete[] quadIndices;

		s_Data->LayoutUniformBuffer = CreateScope<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));
		s_Data->LayoutDescriptorSet = BuildViewDescriptorSet(*s_Data->LayoutUniformBuffer);

		PipelineSpecification specification;
		specification.Layout =
		{
			{ "inPosition", ShaderDataType::Float3,},
			{ "inColor", ShaderDataType::Float4, },
			{ "inTexCoord", ShaderDataType::Float2, },
			{ "inTexIndex", ShaderDataType::Float, },
		};

		specification.DescriptorSets = { s_Data->LayoutDescriptorSet.get() };
		specification.Shader = s_Data->Shader.get();
		specification.RenderPass = s_Data->PipelineRenderPass->GetRenderPass();
		// Sprites: alpha blended, drawn in submission order, visible from both sides (negative scale flips them).
		specification.Blend = BlendMode::Alpha;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;

		s_Data->QuadPipeline = CreateScope<VulkanPipeline>(specification);

		// GPU timing and limits.
		{
			VulkanDevice& device = Renderer::GetVulkanDevice();
			const VkPhysicalDeviceProperties& properties = device.GetPhysicalDevice().GetProperties();
			memcpy(s_Data->GPUName, properties.deviceName, sizeof(s_Data->GPUName));

			if (properties.limits.timestampComputeAndGraphics)
			{
				s_Data->TimestampPeriodNs = properties.limits.timestampPeriod;

				VkQueryPoolCreateInfo queryPoolInfo{};
				queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
				queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
				queryPoolInfo.queryCount = 2 * MAX_FRAMES_IN_FLIGHT;
				VK_CHECK_RESULT(vkCreateQueryPool(device.GetDevice(), &queryPoolInfo, nullptr, &s_Data->TimestampPool));
			}
		}
	}

	void Renderer2D::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// Views belong to the application and must be gone by now (layers release them in OnDetach).
		HY_CORE_ASSERT(s_Data->Views.empty(), "A RenderView still exists at Renderer2D::Shutdown");

		// Resources free themselves through the deletion queue, which Renderer::Shutdown flushes next.
		// Released in the reverse order of Init.
		if (s_Data->TimestampPool != VK_NULL_HANDLE)
		{
			VkDevice device = Renderer::GetVulkanDevice().GetDevice();
			Renderer::SubmitResourceFree([device, pool = s_Data->TimestampPool]()
			{
				vkDestroyQueryPool(device, pool, nullptr);
			});
		}

		s_Data->QuadPipeline.reset();
		s_Data->LayoutDescriptorSet.reset();
		s_Data->LayoutUniformBuffer.reset();
		s_Data->QuadIndexBuffer.reset();
		s_Data->WhiteTexture.reset();
		s_Data->Shader.reset();
		s_Data->PipelineRenderPass.reset();

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

		view.UniformBuffer = CreateScope<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));

		// First batch and texture set. More are created when a frame needs them.
		Renderer2D::AllocateBatch(view);
		Renderer2D::AllocateTextureSet(view);
		Renderer2D::ResetBatches(view);

		s_Data->Views.push_back(&view);
	}

	RenderView::~RenderView()
	{
		RenderViewData& view = *m_Data;

		// Never leave a dangling pointer in the view list or this frame's queue.
		if (s_Data != nullptr)
		{
			auto& views = s_Data->Views;
			views.erase(std::remove(views.begin(), views.end(), &view), views.end());

			auto& queued = s_Data->QueuedViews;
			queued.erase(std::remove(queued.begin(), queued.end(), &view), queued.end());
			if (s_Data->CurrentView == &view)
			{
				s_Data->CurrentView = nullptr;
			}
		}

		for (QuadVertex* base : view.QuadVertexBufferBases)
		{
			delete[] base;
		}

		// The GPU objects (vertex and uniform buffers, descriptor sets, target) free themselves when
		// m_Data goes, through the deletion queue, so frames in flight can still finish with them.
		// The ImGui texture handles belong to ImGui's descriptor pool, which the ImGui layer frees
		// (this ImGui version has no RemoveTexture).
	}

	void RenderView::SetSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

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
		{
			vertexBuffer = CreateScope<VulkanVertexBuffer>(Renderer2DData::MaxVertices * (uint32_t)sizeof(QuadVertex));
		}
		view.QuadVertexBuffers.push_back(std::move(buffers));

		view.QuadIndexCounts.push_back(0);
	}

	void Renderer2D::NextBatch(RenderViewData& view)
	{
		// Current batch is full: move on to the next one, creating it the first time we need it.
		view.BatchIndex++;
		if (view.BatchIndex >= view.QuadVertexBufferBases.size())
		{
			AllocateBatch(view);
		}

		view.QuadIndexCounts[view.BatchIndex] = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[view.BatchIndex];

		// A draw call can't span two vertex buffers.
		StartDraw(view);
	}

	void Renderer2D::AllocateTextureSet(RenderViewData& view)
	{
		TextureSet set;
		set.Slots[0] = s_Data->WhiteTexture;
		set.SlotCount = 1;
		set.DescriptorSet = BuildViewDescriptorSet(*view.UniformBuffer);
		view.TextureSets.push_back(std::move(set));
	}

	void Renderer2D::NextTextureSet(RenderViewData& view)
	{
		// "Flush": the current draw has used every texture slot. Later quads get a new draw call with
		// fresh slots. Their vertices stay in the same batch, and earlier draws are recorded first,
		// so the order sprites were submitted in is kept.
		view.TextureSetIndex++;
		if (view.TextureSetIndex >= view.TextureSets.size())
		{
			AllocateTextureSet(view);
		}

		TextureSet& set = view.TextureSets[view.TextureSetIndex];
		for (uint32_t i = 1; i < set.SlotCount; i++)
		{
			set.Slots[i].reset();
		}
		set.SlotCount = 1;

		StartDraw(view);
		s_Data->Stats.TextureFlushes++;
	}

	float Renderer2D::GetTextureSlot(RenderViewData& view, const Ref<Texture2D>& texture)
	{
		if (texture == nullptr || texture == s_Data->WhiteTexture)
		{
			return 0.0f;
		}

		// At most 31 pointer compares; a texture is usually found within the first few slots.
		TextureSet* set = &view.TextureSets[view.TextureSetIndex];
		for (uint32_t i = 1; i < set->SlotCount; i++)
		{
			if (set->Slots[i] == texture)
			{
				return (float)i;
			}
		}

		if (set->SlotCount == s_Data->TextureSlots)
		{
			NextTextureSet(view);
			set = &view.TextureSets[view.TextureSetIndex];
		}

		const uint32_t slot = set->SlotCount++;
		set->Slots[slot] = texture;
		return (float)slot;
	}

	void Renderer2D::StartDraw(RenderViewData& view)
	{
		QuadDraw draw;
		draw.Batch = view.BatchIndex;
		draw.FirstIndex = view.QuadIndexCounts[view.BatchIndex];
		draw.IndexCount = 0;
		draw.TextureSet = view.TextureSetIndex;

		// A draw nothing was added to yet is reused rather than recorded empty.
		if (view.Draws.empty() == false && view.Draws.back().IndexCount == 0)
		{
			view.Draws.back() = draw;
		}
		else
		{
			view.Draws.push_back(draw);
		}
	}

	void Renderer2D::ResetBatches(RenderViewData& view)
	{
		for (uint32_t& count : view.QuadIndexCounts)
		{
			count = 0;
		}

		view.BatchIndex = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[0];

		// Drop this frame's references to its textures. A texture whose last Ref this was is freed
		// through the deletion queue, after the frames that sample it.
		for (uint32_t s = 0; s <= view.TextureSetIndex && s < view.TextureSets.size(); s++)
		{
			TextureSet& set = view.TextureSets[s];
			for (uint32_t i = 1; i < set.SlotCount; i++)
			{
				set.Slots[i].reset();
			}
			set.SlotCount = 1;
		}
		view.TextureSetIndex = 0;

		view.Draws.clear();
		StartDraw(view);
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
		SubmitQuad(transform, nullptr, 1.0f, color);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		SubmitQuad(transform, texture, tilingFactor, tintColor);
	}

	void Renderer2D::SubmitQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& color)
	{
		HY_CORE_ASSERT(s_Data->CurrentView != nullptr, "Renderer2D::DrawQuad outside BeginScene/EndScene");
		if (s_Data->CurrentView == nullptr)
		{
			return;
		}

		RenderViewData& view = *s_Data->CurrentView;

		constexpr size_t quadVertexCount = 4;
		// Vulkan's v = 0 is the first (top) row of the image, and vertex 0 is the quad's bottom-left
		// corner, so v runs 1 -> 0 from bottom to top and the image appears upright.
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

		// Batch full? Start a new one instead of overflowing (or flushing mid-frame).
		if (view.QuadIndexCounts[view.BatchIndex] >= Renderer2DData::MaxIndices)
		{
			NextBatch(view);
		}

		// May start a new draw call when this draw's texture slots are full.
		const float textureIndex = GetTextureSlot(view, texture);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			view.QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[i];
			view.QuadVertexBufferPtr->Color = color;
			view.QuadVertexBufferPtr->TexCoord = textureCoords[i] * tilingFactor;
			view.QuadVertexBufferPtr->TexIndex = textureIndex;
			view.QuadVertexBufferPtr++;
		}

		view.QuadIndexCounts[view.BatchIndex] += 6;
		view.Draws.back().IndexCount += 6;
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
		// with every view's buffers and descriptor sets for `frame`: safe to overwrite.
		UpdateImGuiTextures(frame);

		// The fence for this frame index was just waited on, so the timestamps this command
		// buffer wrote last time are available: read them, then reset and write new ones.
		const uint32_t firstQuery = frame * 2;
		if (s_Data->TimestampPool)
		{
			if (s_Data->TimestampsWritten[frame])
			{
				uint64_t timestamps[2] = {};
				VkResult result = vkGetQueryPoolResults(Renderer::GetVulkanDevice().GetDevice(),
					s_Data->TimestampPool, firstQuery, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
				if (result == VK_SUCCESS && timestamps[1] >= timestamps[0])
				{
					s_Data->LastGPUTimeMs = (float)((double)(timestamps[1] - timestamps[0]) * s_Data->TimestampPeriodNs / 1.0e6);
				}
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
		s_Data->Stats.MaxTextureSlots = s_Data->TextureSlots;
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

		// Point this frame's descriptor set of every texture set used at its textures. Descriptor
		// sets can't change once bound in a command buffer, so this happens before the pass.
		// A texture released while this frame is in flight is freed through the deletion queue
		// after it, so the GPU never samples a destroyed image.
		for (uint32_t s = 0; s <= view.TextureSetIndex; s++)
		{
			TextureSet& set = view.TextureSets[s];
			WriteTextureSet(set, frame);
			s_Data->Stats.TextureSlotsUsed += set.SlotCount;
		}

		// Upload each batch used this frame. Each batch has its own vertex buffer, so uploading
		// batch i+1 never overwrites vertices that batch i's draws still have to read.
		for (uint32_t i = 0; i <= view.BatchIndex; i++)
		{
			const uint32_t indexCount = view.QuadIndexCounts[i];
			if (indexCount == 0)
			{
				continue;
			}

			const uint32_t vertexCount = (indexCount / 6) * 4;
			view.QuadVertexBuffers[i][frame]->SetData(view.QuadVertexBufferBases[i], vertexCount * (uint32_t)sizeof(QuadVertex));

			s_Data->Stats.BatchCount++;
			s_Data->Stats.VertexBytesUploaded += (uint64_t)vertexCount * sizeof(QuadVertex);
		}

		// One render pass per view per frame => its target is cleared exactly once. Its subpass
		// dependencies make the result visible to ImGui, which samples it later in this command buffer.
		view.Target->Begin(commandBuffer, frame, Renderer::GetSettings().ClearColor);

		// State shared by every draw: bind once. Viewport/scissor follow the view's size.
		s_Data->QuadPipeline->Bind(commandBuffer, { width, height });
		s_Data->QuadIndexBuffer->Bind(commandBuffer);

		// One draw call per draw, in submission order. Vertex buffer and descriptor set are only
		// rebound when they change. Indices are 0,1,2,2,3,0 + 4 * quad, so FirstIndex = 6 * quad
		// with vertex offset 0 reads that quad's vertices.
		uint32_t boundBatch = UINT32_MAX;
		uint32_t boundTextureSet = UINT32_MAX;
		for (const QuadDraw& draw : view.Draws)
		{
			if (draw.IndexCount == 0)
			{
				continue;
			}

			if (draw.Batch != boundBatch)
			{
				view.QuadVertexBuffers[draw.Batch][frame]->Bind(commandBuffer);
				boundBatch = draw.Batch;
			}

			if (draw.TextureSet != boundTextureSet)
			{
				s_Data->QuadPipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[draw.TextureSet].DescriptorSet);
				boundTextureSet = draw.TextureSet;
			}

			vkCmdDrawIndexed(commandBuffer, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
			s_Data->Stats.DrawCalls++;
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

	uint32_t Renderer2D::GetMaxTextureSlots()
	{
		return s_Data != nullptr ? s_Data->TextureSlots : MaxTextureSlots;
	}

	static void WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler)
	{
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();

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
		{
			return;
		}

		// The old images and framebuffers go through the deletion queue: the other frame in flight
		// may still be drawing into or sampling them, so there is no need to wait for the GPU.
		view.Target->Resize(width, height);

		// Each frame index's ImGui texture handle still points at that index's old image, which the
		// frame in flight may be drawing with. Point it at the new one when that index is recorded
		// next (UpdateImGuiTextures); this frame's index now. Updating in place (instead of
		// AddTexture again) means no descriptor sets leak from ImGui's pool.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			view.ImGuiTexturesOutdated[i] = true;
		}
		UpdateImGuiTextures(view, Renderer::GetRenderFrame());
	}

	void Renderer2D::UpdateImGuiTextures(uint32_t frame)
	{
		// Every view, not just the ones begun this frame: ImGui may show a view that isn't drawn.
		for (RenderViewData* view : s_Data->Views)
		{
			UpdateImGuiTextures(*view, frame);
		}
	}

	void Renderer2D::UpdateImGuiTextures(RenderViewData& view, uint32_t frame)
	{
		// Only call this after `frame`'s fence and before the present pass is recorded: the handle
		// must not be in use by a command buffer that is pending or being recorded.
		if (view.ImGuiTexturesOutdated[frame] == false)
		{
			return;
		}

		view.ImGuiTexturesOutdated[frame] = false;
		if (view.ImGuiTextures[frame] != VK_NULL_HANDLE)
		{
			const VulkanImageInfo& image = view.Target->GetImage(frame).GetVulkanImageInfo();
			WriteImGuiTexture(view.ImGuiTextures[frame], image.ImageView, image.Sampler);
		}
	}

	void Renderer2D::ApplySettings()
	{
		const RendererSettings& settings = Renderer::GetSettings();

		// The wireframe flag is baked into the pipeline, so a change means a rebuild.
		// Only rebuild when the GPU can do it; otherwise VulkanPipeline would warn every frame.
		const bool wireframe = settings.Wireframe && Renderer::GetCapabilities().Wireframe;
		if (wireframe != s_Data->QuadPipeline->GetSpecification().Wireframe)
		{
			RebuildQuadPipeline(false, wireframe);
		}

		ReloadShadersIfChanged();
	}

	void Renderer2D::ReloadShadersIfChanged()
	{
		if (Renderer::GetSettings().ShaderHotReload == false)
		{
			return;
		}

		// A few checks per second is instant enough and keeps file-system calls off most frames.
		const auto now = std::chrono::steady_clock::now();
		if (now < s_Data->NextShaderPoll)
		{
			return;
		}

		s_Data->NextShaderPoll = now + Renderer2DData::ShaderPollInterval;
		if (s_Data->ShaderWatcher.Poll() == false)
		{
			return;
		}

		// Poll() has already taken the new file times, so a broken shader is retried on the next save,
		// not every poll.
		if (RebuildQuadPipeline(true, s_Data->QuadPipeline->GetSpecification().Wireframe))
		{
			HY_CORE_INFO("Shaders reloaded.");
		}
	}

	bool Renderer2D::RebuildQuadPipeline(bool recompileShader, bool wireframe)
	{
		// Build the replacements first, so a shader that doesn't compile leaves the old one drawing.
		Scope<VulkanShader> newShader;
		Scope<VulkanPipeline> pipeline;
		try
		{
			if (recompileShader)
			{
				newShader = CreateScope<VulkanShader>(s_Data->Shader->GetSpecification());
			}

			PipelineSpecification specification = s_Data->QuadPipeline->GetSpecification();
			specification.Shader = recompileShader ? newShader.get() : s_Data->Shader.get();
			specification.Wireframe = wireframe;
			pipeline = CreateScope<VulkanPipeline>(specification);
		}
		catch (const std::exception&)
		{
			// VulkanShader already logged the compiler output.
			HY_CORE_WARN("Quad pipeline rebuild failed; still using the previous one.");
			return false;
		}

		// No need to wait for the GPU: the old pipeline frees itself through the deletion queue once
		// the frames in flight that bound it are done. Shader modules aren't used after a pipeline is
		// created, so the old shader can go at once.
		s_Data->QuadPipeline = std::move(pipeline);
		if (recompileShader)
		{
			s_Data->Shader = std::move(newShader);
			s_Data->ShaderWatcher = FileWatcher(s_Data->Shader->GetSourceFiles());
		}

		return true;
	}
}
