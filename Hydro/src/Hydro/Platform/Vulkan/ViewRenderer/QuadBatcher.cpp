#include "hypch.h"
#include "QuadBatcher.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/ShadowMapPass.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptorBuilder.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Renderer/Renderer.h"

#include <cmath>

namespace Hydro
{
	struct QuadBatcherData
	{
		Ref<Texture2D> WhiteTexture;
		Scope<VulkanBuffer> QuadIndexBuffer; // GPU only; every batch starts at vertex 0
	};

	static QuadBatcherData* s_Data = nullptr;

	static const VulkanImageInfo& GetImageInfo(const Texture2D& texture)
	{
		return GetTexture2DData(texture).Image->GetVulkanImageInfo();
	}

	void QuadBatcher::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "QuadBatcher::Init called twice");
		s_Data = new QuadBatcherData();

		// Made in code, so plain coloured quads need no file.
		const uint32_t whitePixel = 0xffffffff;
		ImageSpecification whiteSpecification;
		whiteSpecification.Width = 1;
		whiteSpecification.Height = 1;
		whiteSpecification.Format = ImageFormat::SRGB;
		s_Data->WhiteTexture = CreateRef<Texture2D>(whiteSpecification, &whitePixel);

		std::vector<uint32_t> quadIndices(MaxIndices);
		uint32_t offset = 0;
		for (uint32_t i = 0; i < MaxIndices; i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		BufferSpecification indexSpecification;
		indexSpecification.Usage = BufferUsage::Index;
		indexSpecification.Memory = BufferMemory::GpuOnly;
		indexSpecification.Size = MaxIndices * sizeof(uint32_t);
		indexSpecification.DebugName = "Quad indices";
		s_Data->QuadIndexBuffer = CreateScope<VulkanBuffer>(indexSpecification, quadIndices.data());

		// The texture array has a fixed size in the shader. Vulkan guarantees only 16 samplers per stage, but every
		// desktop GPU has far more; say so clearly if this one doesn't.
		const VkPhysicalDeviceProperties& properties = Renderer::GetVulkanDevice().GetPhysicalDevice().GetProperties();
		const uint32_t samplerLimit = std::min(properties.limits.maxPerStageDescriptorSamplers,
			properties.limits.maxPerStageDescriptorSampledImages);
		if (samplerLimit < MaxTextureSlots)
		{
			HY_CORE_ERROR("Renderer2D: the GPU allows {} textures per shader stage, the quad shader needs {}", samplerLimit,
				MaxTextureSlots);
		}
	}

	void QuadBatcher::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		s_Data->QuadIndexBuffer.reset();
		s_Data->WhiteTexture.reset();
		delete s_Data;
		s_Data = nullptr;
	}

	Scope<VulkanDescriptorBuilder> QuadBatcher::BuildDescriptorSet(const PerFrameBuffers& uniformBuffers,
		const PerFrameBuffers& lightingBuffers,
		const PerFrameBuffers& lightBuffers)
	{
		// Every texture slot starts as the white texture and the shadow map as the empty one; Upload writes the real ones.
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);
		const VulkanImageInfo& emptyShadowMap = ShadowMapPass::GetEmptyMap().GetDepthImage(0)->GetVulkanImageInfo();

		Scope<VulkanDescriptorBuilder> descriptorSet = CreateScope<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindBuffer(0, uniformBuffers, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT);
		descriptorSet->BindImage(1, white.ImageView, white.Sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindBuffer(2, lightingBuffers, sizeof(FrameLightingUniform), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindBuffer(3, lightBuffers, sizeof(GPULight) * MaxForwardLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->BindImage(4, emptyShadowMap.ImageView, ShadowMapPass::GetSampler(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT, 1);
		descriptorSet->Build();
		return descriptorSet;
	}

	VertexBufferLayout QuadBatcher::GetVertexLayout()
	{
		return VertexBufferLayout
		{
			{ "inPosition", ShaderDataType::Float3, },
			{ "inColor", ShaderDataType::Float4, },
			{ "inTexCoord", ShaderDataType::Float2, },
			{ "inTexIndex", ShaderDataType::Float, },
			{ "inEntityID", ShaderDataType::Int, },
			{ "inShadowPosition", ShaderDataType::Float3, },
			{ "inEmission", ShaderDataType::Float3, },
		};
	}

	const Ref<Texture2D>& QuadBatcher::GetWhiteTexture()
	{
		return s_Data->WhiteTexture;
	}

	static void AllocateBatch(RenderViewData& view)
	{
		view.QuadVertexBufferBases.push_back(new QuadVertex[QuadBatcher::MaxVertices]);

		BufferSpecification specification;
		specification.Usage = BufferUsage::Vertex;
		specification.Memory = BufferMemory::HostVisible;
		specification.Size = QuadBatcher::MaxVertices * sizeof(QuadVertex);
		specification.DebugName = view.DebugName + " quad batch " + std::to_string(view.QuadVertexBuffers.size());
		view.QuadVertexBuffers.push_back(CreatePerFrameBuffers(specification));

		view.QuadIndexCounts.push_back(0);
	}

	static void NextBatch(RenderViewData& view)
	{
		view.BatchIndex++;
		if (view.BatchIndex >= view.QuadVertexBufferBases.size())
		{
			AllocateBatch(view);
		}

		view.QuadIndexCounts[view.BatchIndex] = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[view.BatchIndex];

		// A draw call can't span two vertex buffers.
		QuadBatcher::StartDraw(view);
	}

	static void AllocateTextureSet(RenderViewData& view)
	{
		TextureSet set;
		set.Slots[0] = s_Data->WhiteTexture;
		set.SlotCount = 1;
		set.DescriptorSet = QuadBatcher::BuildDescriptorSet(view.UniformBuffers, view.LightingBuffers, view.LightStorageBuffers);
		view.TextureSets.push_back(std::move(set));
	}

	// The current draw has used every texture slot: later quads get a new draw call with fresh slots. Their vertices stay
	// in the same batch and earlier draws are recorded first, so submission order is kept.
	static void NextTextureSet(RenderViewData& view)
	{
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

		QuadBatcher::StartDraw(view);
		ViewRenderer::GetFrameStats().TextureFlushes++;
	}

	// Adds the texture to the current set, or starts a new set (and draw) when it is full.
	static float GetTextureSlot(RenderViewData& view, const Ref<Texture2D>& texture)
	{
		if (texture == nullptr || texture == s_Data->WhiteTexture)
		{
			return 0.0f;
		}

		TextureSet* set = &view.TextureSets[view.TextureSetIndex];
		for (uint32_t i = 1; i < set->SlotCount; i++)
		{
			if (set->Slots[i] == texture)
			{
				return (float)i;
			}
		}

		if (set->SlotCount == MaxTextureSlots)
		{
			NextTextureSet(view);
			set = &view.TextureSets[view.TextureSetIndex];
		}

		const uint32_t slot = set->SlotCount++;
		set->Slots[slot] = texture;
		return (float)slot;
	}

	void QuadBatcher::AddView(RenderViewData& view)
	{
		AllocateBatch(view);
		AllocateTextureSet(view);
		Reset(view);
	}

	void QuadBatcher::StartDraw(RenderViewData& view)
	{
		QuadDraw draw;
		draw.Batch = view.BatchIndex;
		draw.FirstIndex = view.QuadIndexCounts[view.BatchIndex];
		draw.IndexCount = 0;
		draw.TextureSet = view.TextureSetIndex;
		draw.Pickable = view.EntityID >= 0;
		draw.Additive = view.DrawMode == SpriteDrawMode::Additive && view.InOverlay == false && view.InDebugLines == false
			&& view.InShadowCasters == false;

		// A draw nothing was added to yet is reused rather than recorded empty.
		std::vector<QuadDraw>& draws = GetCurrentDraws(view);
		if (draws.empty() == false && draws.back().IndexCount == 0)
		{
			draws.back() = draw;
		}
		else
		{
			draws.push_back(draw);
		}
	}

	void QuadBatcher::Reset(RenderViewData& view)
	{
		for (uint32_t& count : view.QuadIndexCounts)
		{
			count = 0;
		}

		view.BatchIndex = 0;
		view.QuadVertexBufferPtr = view.QuadVertexBufferBases[0];

		// A texture whose last Ref this was is freed through the deletion queue, after the frames that sample it.
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

		view.Lights.clear();
		view.Shadows.clear();
		view.AmbientLight = glm::vec3(0.0f);
		view.HasLights = false;

		view.Draws.clear();
		view.DecalDraws.clear();
		view.CutoutDraws.clear();
		view.FadedCutoutDraws.clear();
		view.OverlayDraws.clear();
		view.InOverlay = false;
		view.DebugDraws.clear();
		view.InDebugLines = false;
		view.ShadowCasterDraws.clear();
		view.InShadowCasters = false;
		view.PostProcess = ScenePostProcess{};
		view.TonemapSet = VK_NULL_HANDLE;
		view.BloomDrawn = false;
		view.DofDrawn = false;
		view.HasFocus = false;
		view.Lighting = SceneLighting{};
		view.LightingSubmitted = false;
		view.DrawMode = SpriteDrawMode::Translucent;
		view.EntityID = -1;
		view.Emission = glm::vec3(0.0f);
		view.MeshDraws.clear();
		StartDraw(view);
	}

	std::vector<QuadDraw>& QuadBatcher::GetCurrentDraws(RenderViewData& view)
	{
		if (view.InShadowCasters)
		{
			return view.ShadowCasterDraws;
		}
		if (view.InDebugLines)
		{
			return view.DebugDraws;
		}
		if (view.InOverlay)
		{
			return view.OverlayDraws;
		}
		switch (view.DrawMode)
		{
			case SpriteDrawMode::Decal: return view.DecalDraws;
			case SpriteDrawMode::Cutout: return view.CutoutDraws;
			case SpriteDrawMode::FadedCutout: return view.FadedCutoutDraws;
			default: return view.Draws;
		}
	}

	void QuadBatcher::Submit(RenderViewData& view, const glm::vec3 (&corners)[4], const Ref<Texture2D>& texture,
		const TextureRect& textureRect,
		const glm::vec4& color, const glm::vec3 (*shadowCorners)[4])
	{
		// Screen space (BeginScreenOverlay): each corner back through the scene's view-projection. The corners all sit at
		// the same depth, so the quad is drawn without perspective, exactly over the screen pixels asked for.
		glm::vec3 screenCorners[4];
		const glm::vec3 (*drawnCorners)[4] = &corners;
		if (view.InScreenSpace)
		{
			if (view.ScreenSpaceValid == false)
			{
				return;
			}
			for (size_t i = 0; i < 4; i++)
			{
				const glm::dvec4 world = view.ScreenToWorld * glm::dvec4(glm::dvec3(corners[i]), 1.0);
				if (std::abs(world.w) <= 1.0e-12)
				{
					return;
				}
				screenCorners[i] = glm::vec3(glm::dvec3(world) / world.w);
			}
			drawnCorners = &screenCorners;
		}

		// Vulkan's v = 0 is the image's top row and vertex 0 the quad's bottom-left, so v runs Max.y to Min.y upwards.
		const glm::vec2 textureCoords[] = {
			{ textureRect.Min.x, textureRect.Max.y },
			{ textureRect.Max.x, textureRect.Max.y },
			{ textureRect.Max.x, textureRect.Min.y },
			{ textureRect.Min.x, textureRect.Min.y } };

		if (view.QuadIndexCounts[view.BatchIndex] >= MaxIndices)
		{
			NextBatch(view);
		}

		// May start a new draw call when this draw's texture slots are full.
		const float textureIndex = GetTextureSlot(view, texture);

		for (size_t i = 0; i < 4; i++)
		{
			view.QuadVertexBufferPtr->Position = (*drawnCorners)[i];
			view.QuadVertexBufferPtr->Color = color;
			view.QuadVertexBufferPtr->TexCoord = textureCoords[i];
			view.QuadVertexBufferPtr->TexIndex = textureIndex;
			view.QuadVertexBufferPtr->EntityID = view.EntityID;
			view.QuadVertexBufferPtr->ShadowPosition = shadowCorners != nullptr ? (*shadowCorners)[i] : (*drawnCorners)[i];
			view.QuadVertexBufferPtr->Emission = view.Emission;
			view.QuadVertexBufferPtr++;
		}

		view.QuadIndexCounts[view.BatchIndex] += 6;
		GetCurrentDraws(view).back().IndexCount += 6;
		Statistics& stats = ViewRenderer::GetFrameStats();
		stats.QuadCount++;
		if (view.InShadowCasters)
		{
			stats.ShadowCasterQuads++;
		}
	}

	// Points the texture array of `set`'s descriptor set for `frame` at its textures (unused slots: white).
	static void WriteTextureSet(TextureSet& set, uint32_t frame)
	{
		const VulkanImageInfo& white = GetImageInfo(*s_Data->WhiteTexture);

		std::array<VkDescriptorImageInfo, MaxTextureSlots> images{};
		for (uint32_t i = 0; i < MaxTextureSlots; i++)
		{
			const VulkanImageInfo& image = (i < set.SlotCount && set.Slots[i] != nullptr) ? GetImageInfo(*set.Slots[i]) : white;
			images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			images[i].imageView = image.ImageView;
			images[i].sampler = image.Sampler;
		}

		set.DescriptorSet->WriteImages(frame, 1, images.data(), MaxTextureSlots, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	// Binding 4: the view's sun shadow map when it draws one this frame, else the empty one.
	static void WriteShadowMapBinding(TextureSet& set, const RenderViewData& view, uint32_t frame)
	{
		const VulkanRenderTarget& empty = ShadowMapPass::GetEmptyMap();
		const VulkanRenderTarget& map = (view.SunShadowsActive && view.ShadowTarget != nullptr) ? *view.ShadowTarget : empty;
		const uint32_t mapFrame = &map == &empty ? 0 : frame;
		VkDescriptorImageInfo image{};
		image.sampler = ShadowMapPass::GetSampler();
		image.imageView = map.GetDepthImage(mapFrame)->GetVulkanImageInfo().ImageView;
		image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		set.DescriptorSet->WriteImages(frame, 4, &image, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	void QuadBatcher::Upload(RenderViewData& view, uint32_t frame)
	{
		Statistics& stats = ViewRenderer::GetFrameStats();

		// A texture released while this frame is in flight is freed after it, so the GPU never samples a destroyed image.
		for (uint32_t s = 0; s <= view.TextureSetIndex; s++)
		{
			TextureSet& set = view.TextureSets[s];
			WriteTextureSet(set, frame);
			WriteShadowMapBinding(set, view, frame);
			stats.TextureSlotsUsed += set.SlotCount;
		}

		// Each batch has its own vertex buffer, so uploading batch i+1 never overwrites vertices batch i still reads.
		for (uint32_t i = 0; i <= view.BatchIndex; i++)
		{
			const uint32_t indexCount = view.QuadIndexCounts[i];
			if (indexCount == 0)
			{
				continue;
			}

			const uint32_t vertexCount = (indexCount / 6) * 4;
			view.QuadVertexBuffers[i][frame]->SetData(view.QuadVertexBufferBases[i], vertexCount * sizeof(QuadVertex));

			stats.BatchCount++;
			stats.FullestBatchQuads = std::max(stats.FullestBatchQuads, indexCount / 6);
			stats.VertexBytesUploaded += (uint64_t)vertexCount * sizeof(QuadVertex);
		}
	}

	void QuadBatcher::BindIndexBuffer(VkCommandBuffer commandBuffer)
	{
		s_Data->QuadIndexBuffer->BindAsIndexBuffer(commandBuffer);
	}

	void QuadBatcher::Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame, const std::vector<QuadDraw>& draws,
		SpriteDrawMode mode, bool overlay, bool debugLines)
	{
		const bool hasDraws = std::any_of(draws.begin(), draws.end(), [](const QuadDraw& draw) { return draw.IndexCount != 0; });
		if (hasDraws == false)
		{
			return;
		}

		ViewPipelineKind kind = ViewPipelineKind::Sprite;
		if (debugLines)
		{
			kind = ViewPipelineKind::DebugLines;
		}
		else if (overlay)
		{
			kind = ViewPipelineKind::Overlay;
		}
		else if (mode == SpriteDrawMode::Cutout)
		{
			kind = ViewPipelineKind::Cutout;
		}
		else if (mode == SpriteDrawMode::FadedCutout)
		{
			kind = ViewPipelineKind::FadedCutout;
		}
		else if (mode == SpriteDrawMode::Decal)
		{
			kind = ViewPipelineKind::Decal;
		}

		// With the entity-ID attachment, pickable draws (an entity's sprites, HUD sprites too) write their IDs and the
		// rest (particles, lines, outlines) use a variant that leaves them alone. The overlay and debug lines stay unlit.
		const bool hasEntityID = view.Target->GetSpecification().EntityID;
		const bool lit = view.Features.Lighting == LightingMode::Forward && view.LightingSubmitted && overlay == false
			&& debugLines == false;
		VulkanPipeline* pickablePipeline = ViewPipelineCache::Get(kind, *view.Target, debugLines == false, lit, view.PassColorTarget);
		VulkanPipeline* otherPipeline = hasEntityID ? ViewPipelineCache::Get(kind, *view.Target, false, lit, view.PassColorTarget)
			: pickablePipeline;
		// Additive draws sit among the translucent ones, so they keep the back-to-front order.
		const bool anyAdditive = kind == ViewPipelineKind::Sprite
			&& std::any_of(draws.begin(), draws.end(), [](const QuadDraw& draw) { return draw.Additive && draw.IndexCount != 0; });
		VulkanPipeline* additivePickablePipeline = anyAdditive
			? ViewPipelineCache::Get(ViewPipelineKind::Additive, *view.Target, true, false, view.PassColorTarget) : nullptr;
		VulkanPipeline* additiveOtherPipeline = anyAdditive && hasEntityID
			? ViewPipelineCache::Get(ViewPipelineKind::Additive, *view.Target, false, false, view.PassColorTarget)
			: additivePickablePipeline;

		// The pass before may have bound another index buffer (the meshes).
		BindIndexBuffer(commandBuffer);
		VulkanPipeline* pipeline = nullptr;

		// Indices are 0,1,2,2,3,0 + 4 * quad, so FirstIndex = 6 * quad with vertex offset 0 reads that quad's vertices.
		uint32_t boundBatch = UINT32_MAX;
		uint32_t boundTextureSet = UINT32_MAX;
		Statistics& stats = ViewRenderer::GetFrameStats();
		for (const QuadDraw& draw : draws)
		{
			VulkanPipeline* drawPipeline = draw.Pickable ? pickablePipeline : otherPipeline;
			if (anyAdditive && draw.Additive)
			{
				drawPipeline = draw.Pickable ? additivePickablePipeline : additiveOtherPipeline;
			}
			if (draw.IndexCount == 0 || drawPipeline == nullptr)
			{
				continue;
			}

			if (drawPipeline != pipeline)
			{
				pipeline = drawPipeline;
				pipeline->Bind(commandBuffer, view.Target->GetExtent());
				boundTextureSet = UINT32_MAX;
			}

			if (draw.Batch != boundBatch)
			{
				view.QuadVertexBuffers[draw.Batch][frame]->BindAsVertexBuffer(commandBuffer);
				boundBatch = draw.Batch;
			}

			if (draw.TextureSet != boundTextureSet)
			{
				pipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[draw.TextureSet].DescriptorSet);
				boundTextureSet = draw.TextureSet;
			}

			vkCmdDrawIndexed(commandBuffer, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
			stats.DrawCalls++;
		}
	}
}
