#include "hypch.h"
#include "BloomPass.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	// At most this many levels, each half the previous, none smaller than BloomMinLevelSize on either side.
	static constexpr uint32_t BloomMaxLevels = 6;
	static constexpr uint32_t BloomMinLevelSize = 8;

	// One fullscreen step: `source` (sampled) into `target`, cleared or added onto (`load`). Leaves `target` readable.
	// False when it couldn't be recorded (no pipeline after a failed hot reload, or no descriptor set).
	static bool RecordStep(VkCommandBuffer commandBuffer, uint32_t frame, ViewPipelineKind kind, const VulkanImage& source,
		VulkanRenderTarget& target, bool load, const glm::vec2& extra)
	{
		VulkanPipeline* pipeline = ViewPipelineCache::Get(kind, target, false);
		VkDescriptorSet set = VulkanDescriptorAllocator::AllocateFrame(ViewPipelineCache::GetBloomLayout());
		if (pipeline == nullptr || set == VK_NULL_HANDLE)
		{
			return false;
		}

		const VulkanImageInfo& image = source.GetVulkanImageInfo();
		VkDescriptorImageInfo imageInfo{ image.Sampler, image.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 1, &write, 0, nullptr);

		BloomPushConstants constants;
		constants.Params = glm::vec4(1.0f / (float)source.GetWidth(), 1.0f / (float)source.GetHeight(), extra.x, extra.y);

		RenderTargetBeginInfo begin = RenderTargetBeginInfo::Clear(glm::vec4(0.0f));
		begin.ColorLoad = load ? AttachmentLoadOp::Load : AttachmentLoadOp::DontCare; // every pixel is drawn
		target.Begin(commandBuffer, frame, begin);
		pipeline->Bind(commandBuffer, target.GetExtent());
		pipeline->BindDescriptorSet(commandBuffer, 0, set);
		pipeline->PushConstants(commandBuffer, &constants, (uint32_t)sizeof(constants));
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		target.End(commandBuffer, frame);
		ViewRenderer::GetFrameStats().DrawCalls++;
		return true;
	}

	void BloomPass::Validate(const RenderGraphContext&, std::vector<RenderValidationMessage>& out) const
	{
		if (m_HDR == false)
		{
			AddMessage(out, RenderValidationSeverity::Error, "bloom needs HDR");
		}
	}

	void BloomPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("HDRColor");
		AddRecordedPass(builder, "Bloom", RenderStage::PostProcess, { "HDRColor" }, { "BloomTexture" });
	}

	void BloomPass::Record(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		view.BloomDrawn = false;
		const ScenePostProcess& settings = view.PostProcess;
		if (view.SceneColor == nullptr || settings.Bloom == false || settings.BloomIntensity <= 0.0f)
		{
			// Off for this view: don't keep its chain (#157). The images go through the deletion queue.
			view.BloomLevels.clear();
			return;
		}

		// Made again (old images through the deletion queue) when the view's size changes.
		const uint32_t width = view.SceneColor->GetWidth();
		const uint32_t height = view.SceneColor->GetHeight();
		uint32_t levelCount = 0;
		while (levelCount < BloomMaxLevels && std::min(width, height) >> (levelCount + 1) >= BloomMinLevelSize)
		{
			levelCount++;
		}
		if (levelCount == 0)
		{
			return; // a view too small to bloom
		}
		const bool sizeMatches = view.BloomLevels.size() == levelCount && view.BloomLevels[0]->GetWidth() == std::max(width >> 1, 1u)
			&& view.BloomLevels[0]->GetHeight() == std::max(height >> 1, 1u);
		if (sizeMatches == false)
		{
			view.BloomLevels.clear();
			for (uint32_t level = 0; level < levelCount; level++)
			{
				RenderTargetSpecification specification;
				specification.Width = std::max(width >> (level + 1), 1u);
				specification.Height = std::max(height >> (level + 1), 1u);
				specification.ColorFormats = { SceneColorFormat };
				specification.DebugName = view.DebugName + " bloom " + std::to_string(level);
				view.BloomLevels.push_back(CreateScope<VulkanRenderTarget>(specification));
			}
		}

		// Prefilter: the scene colour into level 0, a 13-tap downsample (Karis-weighted against fireflies) with the soft
		// threshold. Then each level into the next, then back up, each level filtered and added onto the one above it.
		VkCommandBuffer commandBuffer = context.CommandBuffer;
		const uint32_t frame = context.Frame;
		const float threshold = std::max(settings.BloomThreshold, 0.0f);
		const float knee = std::clamp(settings.BloomKnee, 0.0f, 1.0f) * threshold;
		// Each step reads what the one before wrote, so the chain stops at the first step that can't be recorded, and the
		// tonemap only samples level 0 when every step ran (#156): otherwise it could read an image never written.
		if (RecordStep(commandBuffer, frame, ViewPipelineKind::BloomPrefilter, view.SceneColor->GetImage(frame), *view.BloomLevels[0],
				false, glm::vec2(threshold, knee))
			== false)
		{
			return;
		}
		for (uint32_t level = 1; level < levelCount; level++)
		{
			if (RecordStep(commandBuffer, frame, ViewPipelineKind::BloomDown, view.BloomLevels[level - 1]->GetImage(frame),
					*view.BloomLevels[level], false, glm::vec2(0.0f))
				== false)
			{
				return;
			}
		}
		const float radius = std::clamp(settings.BloomRadius, 0.25f, 4.0f);
		for (uint32_t level = levelCount - 1; level > 0; level--)
		{
			if (RecordStep(commandBuffer, frame, ViewPipelineKind::BloomUp, view.BloomLevels[level]->GetImage(frame),
					*view.BloomLevels[level - 1], true, glm::vec2(radius, 0.0f))
				== false)
			{
				return;
			}
		}

		view.BloomDrawn = true;
		Statistics& stats = ViewRenderer::GetFrameStats();
		stats.BloomViewCount++;
		stats.BloomLevelCount = levelCount;
		stats.BloomWidth = view.BloomLevels[0]->GetWidth();
		stats.BloomHeight = view.BloomLevels[0]->GetHeight();
	}
}
