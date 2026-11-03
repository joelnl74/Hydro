#include "hypch.h"
#include "DepthOfFieldPass.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	// The gather blur takes at most this many samples (MaxSamples in dof_blur.frag).
	static constexpr uint32_t DepthOfFieldMaxSamples = 128;

	// The view's depth is read through it: a depth format need not support linear filtering.
	static VkSampler s_NearestSampler = VK_NULL_HANDLE;

	void DepthOfFieldPass::Init()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 0.0f;
		VK_CHECK_RESULT(vkCreateSampler(Renderer::GetVulkanDevice().GetDevice(), &samplerInfo, nullptr, &s_NearestSampler));
	}

	void DepthOfFieldPass::Shutdown()
	{
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, sampler = s_NearestSampler]()
		{
			vkDestroySampler(device, sampler, nullptr);
		});
		s_NearestSampler = VK_NULL_HANDLE;
	}

	void DepthOfFieldPass::Release(RenderViewData& view)
	{
		view.DofCoc.reset();
		view.DofHalf.reset();
		view.DofBlur.reset();
	}

	// An image as a fragment shader samples it (left in SHADER_READ_ONLY_OPTIMAL by its target's End).
	static VkDescriptorImageInfo GetSampledImage(const VulkanImage& image)
	{
		const VulkanImageInfo& info = image.GetVulkanImageInfo();
		return { info.Sampler, info.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	// Made again (old images through the deletion queue) when missing or its size changed.
	static void PrepareTarget(Scope<VulkanRenderTarget>& target, uint32_t width, uint32_t height, ImageFormat format,
		const std::string& debugName)
	{
		if (target != nullptr && target->GetWidth() == width && target->GetHeight() == height)
		{
			return;
		}
		RenderTargetSpecification specification;
		specification.Width = width;
		specification.Height = height;
		specification.ColorFormats = { format };
		specification.DebugName = debugName;
		target = CreateScope<VulkanRenderTarget>(specification);
	}

	// One fullscreen step: `sources` (sampled) into `target`, every pixel written. Leaves `target` readable. False when it
	// couldn't be recorded (no pipeline after a failed hot reload, or no descriptor set).
	static bool RecordStep(VkCommandBuffer commandBuffer, uint32_t frame, ViewPipelineKind kind, const VkDescriptorImageInfo (&sources)[2],
		VulkanRenderTarget& target, const DepthOfFieldPushConstants& constants)
	{
		VulkanPipeline* pipeline = ViewPipelineCache::Get(kind, target, false);
		VkDescriptorSet set = VulkanDescriptorAllocator::AllocateFrame(ViewPipelineCache::GetDepthOfFieldLayout());
		if (pipeline == nullptr || set == VK_NULL_HANDLE)
		{
			return false;
		}

		VkWriteDescriptorSet writes[2]{};
		for (uint32_t i = 0; i < 2; i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = set;
			writes[i].dstBinding = i;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].descriptorCount = 1;
			writes[i].pImageInfo = &sources[i];
		}
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 2, writes, 0, nullptr);

		RenderTargetBeginInfo begin = RenderTargetBeginInfo::Clear(glm::vec4(0.0f));
		begin.ColorLoad = AttachmentLoadOp::DontCare; // every pixel is drawn
		target.Begin(commandBuffer, frame, begin);
		pipeline->Bind(commandBuffer, target.GetExtent());
		pipeline->BindDescriptorSet(commandBuffer, 0, set);
		pipeline->PushConstants(commandBuffer, &constants, (uint32_t)sizeof(constants));
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		target.End(commandBuffer, frame);
		ViewRenderer::GetFrameStats().DrawCalls++;
		return true;
	}

	void DepthOfFieldPass::Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const
	{
		if (m_HDR == false)
		{
			AddMessage(out, RenderValidationSeverity::Error, "depth of field needs HDR");
		}
		else if (context.Settings.Depth != DepthMode::Shared)
		{
			AddMessage(out, RenderValidationSeverity::Error, "depth of field needs Depth = Shared");
		}
	}

	void DepthOfFieldPass::Setup(RenderGraphBuilder& builder)
	{
		// The scene pass's End leaves the depth readable.
		builder.RequireSampledDepth();
		builder.ImportResource("HDRColor");
		builder.ImportResource("Depth");
		AddRecordedPass(builder, PassName, RenderStage::PostProcess, { "HDRColor", "Depth" }, { "DepthOfFieldTexture" });
	}

	void DepthOfFieldPass::Record(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		view.DofDrawn = false;
		const ScenePostProcess& settings = view.PostProcess;
		const VulkanImage* depth = view.Target->GetDepthImage(context.Frame);
		if (view.SceneColor == nullptr || depth == nullptr || view.Target->GetSpecification().SampledDepth == false
			|| settings.DepthOfField == false)
		{
			Release(view);
			return;
		}
		if (view.HasFocus == false)
		{
			return; // nothing to measure from this frame (no camera): drawn sharp
		}

		// The largest radius in the view's pixels, from the scene's percent of its height. Under a pixel: nothing to blur.
		const uint32_t width = view.SceneColor->GetWidth();
		const uint32_t height = view.SceneColor->GetHeight();
		const float maxRadius = std::clamp(settings.DofMaxRadius, 0.0f, 5.0f) * 0.01f * (float)height;
		if (maxRadius < 1.0f || width < 4 || height < 4)
		{
			Release(view);
			return;
		}
		const uint32_t halfWidth = std::max(width >> 1, 1u);
		const uint32_t halfHeight = std::max(height >> 1, 1u);
		PrepareTarget(view.DofCoc, width, height, ImageFormat::R16F, view.DebugName + " depth of field CoC");
		PrepareTarget(view.DofHalf, halfWidth, halfHeight, SceneColorFormat, view.DebugName + " depth of field half");
		PrepareTarget(view.DofBlur, halfWidth, halfHeight, SceneColorFormat, view.DebugName + " depth of field blur");

		// Measured along the view direction from the focus point (Renderer2D::SetViewFocus), in front and behind.
		DepthOfFieldPushConstants constants;
		constants.InverseViewProjection = glm::inverse(view.ViewProjection);
		constants.Focus = glm::vec4(view.FocusForward, glm::dot(view.FocusPoint, view.FocusForward));
		constants.Band = glm::vec4(std::max(settings.DofNearStart, 0.0f), std::max(settings.DofFarStart, 0.0f),
			1.0f / std::max(settings.DofRange, 0.01f), maxRadius);

		// As for the bloom, the chain stops at the first step that can't be recorded, and the tonemap only mixes the blur
		// in when every step ran (#156).
		VkCommandBuffer commandBuffer = context.CommandBuffer;
		const uint32_t frame = context.Frame;

		// 1. The circle of confusion from the depth, through the nearest sampler (binding 1 repeats it, unread).
		const VkDescriptorImageInfo depthInfo{ s_NearestSampler, depth->GetVulkanImageInfo().ImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		const VkDescriptorImageInfo cocSources[2] = { depthInfo, depthInfo };
		constants.Texel = glm::vec4(1.0f / (float)width, 1.0f / (float)height, maxRadius, 0.0f);
		if (RecordStep(commandBuffer, frame, ViewPipelineKind::DofCoc, cocSources, *view.DofCoc, constants) == false)
		{
			return;
		}

		// 2. The scene colour and its circle of confusion at half size.
		const VkDescriptorImageInfo prepareSources[2] = { GetSampledImage(view.SceneColor->GetImage(frame)),
			GetSampledImage(view.DofCoc->GetImage(frame)) };
		if (RecordStep(commandBuffer, frame, ViewPipelineKind::DofPrepare, prepareSources, *view.DofHalf, constants) == false)
		{
			return;
		}

		// 3. The gather blur. Its spiral takes about radius^2 / (2 step) samples to reach the radius: the step grows with
		//    the radius so it stays under DepthOfFieldMaxSamples (with a margin), and is never under half a pixel.
		const float halfRadius = maxRadius * 0.5f;
		const float step = std::max(0.5f, halfRadius * halfRadius / (2.0f * 0.9f * (float)DepthOfFieldMaxSamples));
		constants.Texel = glm::vec4(1.0f / (float)halfWidth, 1.0f / (float)halfHeight, halfRadius, step);
		const VkDescriptorImageInfo half = GetSampledImage(view.DofHalf->GetImage(frame));
		const VkDescriptorImageInfo blurSources[2] = { half, half };
		if (RecordStep(commandBuffer, frame, ViewPipelineKind::DofBlur, blurSources, *view.DofBlur, constants) == false)
		{
			return;
		}

		view.DofDrawn = true;
		Statistics& stats = ViewRenderer::GetFrameStats();
		stats.DepthOfFieldViewCount++;
		stats.DepthOfFieldWidth = halfWidth;
		stats.DepthOfFieldHeight = halfHeight;
	}
}
