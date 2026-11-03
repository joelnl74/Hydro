#include "hypch.h"
#include "TonemapPass.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Renderer/Renderer.h"

#include <cmath>

namespace Hydro
{
	static VkDescriptorImageInfo GetSampledImage(const VulkanImage& image)
	{
		const VulkanImageInfo& info = image.GetVulkanImageInfo();
		return { info.Sampler, info.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	static TonemapPushConstants MakeConstants(const ScenePostProcess& settings, float bloomIntensity, bool depthOfField)
	{
		TonemapPushConstants constants;
		constants.Params = glm::vec4(std::exp2(std::clamp(settings.Exposure, -16.0f, 16.0f)), bloomIntensity, depthOfField ? 1.0f : 0.0f,
			settings.Grading ? 1.0f : 0.0f);
		constants.Vignette = glm::vec4(std::clamp(settings.VignetteIntensity, 0.0f, 1.0f), std::clamp(settings.VignetteRadius, 0.0f, 1.5f),
			std::max(settings.VignetteSoftness, 0.01f), 0.0f);
		if (settings.Grading)
		{
			// White balance: temperature moves red against blue, tint green against magenta, then scaled back to the same
			// brightness (Rec. 709 luma), so only the hue moves.
			const float temperature = std::clamp(settings.Temperature, -1.0f, 1.0f);
			const float tint = std::clamp(settings.Tint, -1.0f, 1.0f);
			glm::vec3 balance(1.0f + 0.2f * temperature + 0.1f * tint, 1.0f - 0.2f * tint, 1.0f - 0.2f * temperature + 0.1f * tint);
			balance /= glm::dot(balance, glm::vec3(0.2126f, 0.7152f, 0.0722f));
			constants.WhiteBalance = glm::vec4(balance, std::clamp(settings.Contrast, 0.1f, 4.0f));
			constants.Lift = glm::vec4(glm::clamp(settings.Lift, glm::vec3(-1.0f), glm::vec3(1.0f)), std::max(settings.Saturation, 0.0f));
			constants.InverseGamma = glm::vec4(1.0f / glm::max(settings.Gamma, glm::vec3(0.05f)), 0.0f);
			constants.Gain = glm::vec4(glm::max(settings.Gain, glm::vec3(0.0f)), 0.0f);
		}
		return constants;
	}

	void TonemapPass::Setup(RenderGraphBuilder& builder)
	{
		// The overlay and debug lines are drawn after it, untouched by exposure and the tonemap.
		builder.RequireSceneColor(SceneColorFormat);
		builder.ImportResource("HDRColor");
		builder.ImportResource("SceneColor");
		std::vector<std::string> reads = { "HDRColor" };
		if (m_Bloom)
		{
			reads.push_back("BloomTexture");
		}
		if (m_DepthOfField)
		{
			reads.push_back("DepthOfFieldTexture");
		}
		AddRecordedPass(builder, PassName, RenderStage::Resolve, reads, { "SceneColor" });
	}

	void TonemapPass::Record(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.SceneColor == nullptr)
		{
			return;
		}
		VulkanPipeline* pipeline = ViewPipelineCache::Get(ViewPipelineKind::Tonemap, *view.Target, false);
		if (pipeline == nullptr)
		{
			return;
		}

		// The half-size images (bloom level 0, the blur, the half colour whose alpha feeds the depth-aware upsample) are
		// filtered. What wasn't drawn this frame repeats the scene colour, unread.
		const VulkanImage& image = view.SceneColor->GetImage(context.Frame);
		const VulkanImage& bloom = view.BloomDrawn ? view.BloomLevels[0]->GetImage(context.Frame) : image;
		const VulkanImage& circleOfConfusion = view.DofDrawn ? view.DofCoc->GetImage(context.Frame) : image;
		const VulkanImage& depthOfField = view.DofDrawn ? view.DofBlur->GetImage(context.Frame) : image;
		const VulkanImage& depthOfFieldHalf = view.DofDrawn ? view.DofHalf->GetImage(context.Frame) : image;
		view.TonemapSet = VulkanDescriptorAllocator::AllocateFrame(ViewPipelineCache::GetTonemapLayout());
		if (view.TonemapSet == VK_NULL_HANDLE)
		{
			return;
		}
		const VkDescriptorImageInfo imageInfos[5] = { GetSampledImage(image), GetSampledImage(bloom), GetSampledImage(circleOfConfusion),
			GetSampledImage(depthOfField), GetSampledImage(depthOfFieldHalf) };
		VkWriteDescriptorSet writes[5]{};
		for (uint32_t i = 0; i < 5; i++)
		{
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = view.TonemapSet;
			writes[i].dstBinding = i;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].descriptorCount = 1;
			writes[i].pImageInfo = &imageInfos[i];
		}
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 5, writes, 0, nullptr);

		// Level 0 holds every level added together (the up chain): divided by the count for one level's worth.
		const float bloomIntensity = view.BloomDrawn ? std::max(view.PostProcess.BloomIntensity, 0.0f) / (float)view.BloomLevels.size()
			: 0.0f;
		const TonemapPushConstants constants = MakeConstants(view.PostProcess, bloomIntensity, view.DofDrawn);

		pipeline->Bind(context.CommandBuffer, view.Target->GetExtent());
		pipeline->BindDescriptorSet(context.CommandBuffer, 0, view.TonemapSet);
		pipeline->PushConstants(context.CommandBuffer, &constants, (uint32_t)sizeof(constants));
		vkCmdDraw(context.CommandBuffer, 3, 1, 0, 0);
		Statistics& stats = ViewRenderer::GetFrameStats();
		stats.DrawCalls++;
		stats.HDRViewCount++;
	}
}
