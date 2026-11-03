#include "hypch.h"
#include "ShadowMapPass.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer3D.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Hydro
{
	// How far beyond the shadowed area (towards the sun) the shadow camera starts, so casters outside it (a tall tree
	// or a cliff up the hill) still throw their shadows into it. Metres.
	static constexpr float ShadowCasterReach = 60.0f;

	struct ShadowMapPassData
	{
		VkSampler Sampler = VK_NULL_HANDLE;
		Scope<VulkanRenderTarget> EmptyMap; // cleared to "nothing in the way"; the lit shaders always read binding 4
	};

	static ShadowMapPassData* s_Data = nullptr;

	void ShadowMapPass::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "ShadowMapPass::Init called twice");
		s_Data = new ShadowMapPassData();

		// Linear: the hardware compares four texels and blends the results, 2 x 2 PCF per tap.
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.compareEnable = VK_TRUE;
		samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // lit where the surface is no further than what the map holds
		samplerInfo.maxLod = 0.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(Renderer::GetVulkanDevice().GetDevice(), &samplerInfo, nullptr, &s_Data->Sampler));

		RenderTargetSpecification emptySpecification;
		emptySpecification.ColorFormats = {};
		emptySpecification.DepthFormat = ShadowMapFormat;
		emptySpecification.SampledDepth = true;
		emptySpecification.DebugName = "Empty shadow map";
		s_Data->EmptyMap = CreateScope<VulkanRenderTarget>(emptySpecification);
		Renderer::GetVulkanDevice().SubmitImmediate([](VkCommandBuffer commandBuffer)
		{
			for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++)
			{
				s_Data->EmptyMap->Begin(commandBuffer, frame, RenderTargetBeginInfo::Clear(glm::vec4(0.0f)));
				s_Data->EmptyMap->End(commandBuffer, frame);
			}
		});
	}

	void ShadowMapPass::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		s_Data->EmptyMap.reset();
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, sampler = s_Data->Sampler]()
		{
			vkDestroySampler(device, sampler, nullptr);
		});
		delete s_Data;
		s_Data = nullptr;
	}

	VkSampler ShadowMapPass::GetSampler()
	{
		return s_Data->Sampler;
	}

	const VulkanRenderTarget& ShadowMapPass::GetEmptyMap()
	{
		return *s_Data->EmptyMap;
	}

	void ShadowMapPass::Prepare(RenderViewData& view)
	{
		const SceneLighting& lighting = view.Lighting;
		view.SunShadowsActive = view.Graph != nullptr && view.Graph->HasPass(PassName) && view.LightingSubmitted
			&& lighting.HasSun && lighting.SunShadows && glm::length(lighting.SunDirection) > 0.0f;
		if (view.SunShadowsActive == false)
		{
			return;
		}

		// Remade (old images through the deletion queue) when its size setting changes.
		const uint32_t size = std::clamp(Renderer::GetSettings().SunShadowMapSize, 512u, 4096u);
		if (view.ShadowTarget == nullptr)
		{
			RenderTargetSpecification specification;
			specification.Width = size;
			specification.Height = size;
			specification.ColorFormats = {};
			specification.DepthFormat = ShadowMapFormat;
			specification.SampledDepth = true;
			specification.DebugName = view.DebugName + " sun shadow map";
			view.ShadowTarget = CreateScope<VulkanRenderTarget>(specification);
		}
		else
		{
			view.ShadowTarget->Resize(size, size);
		}

		// Looks along the sunlight at what the view's camera looks at (FogOrigin), covering ShadowDistance metres each
		// way, starting ShadowCasterReach metres further towards the sun.
		const float distance = std::max(lighting.ShadowDistance, 1.0f);
		const glm::vec3 travel = glm::normalize(lighting.SunDirection);
		const glm::vec3 up = std::abs(travel.z) > 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::vec3 centre = lighting.FogOrigin;
		const float back = distance + ShadowCasterReach;
		const glm::mat4 lightView = glm::lookAt(centre - travel * back, centre, up);
		glm::mat4 projection = Camera::Orthographic(-distance, distance, -distance, distance, 0.0f, back + distance);

		// Texel snapping: the world's origin lands on a texel corner, so the map slides over the world in whole texels as
		// the camera moves and shadow edges stay put instead of crawling.
		const glm::mat4 matrix = projection * lightView;
		const float halfSize = (float)size * 0.5f;
		const glm::vec4 origin = matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const glm::vec2 originTexels = glm::vec2(origin) * halfSize;
		const glm::vec2 offset = (glm::round(originTexels) - originTexels) / halfSize;
		projection[3][0] += offset.x;
		projection[3][1] += offset.y;
		view.ShadowMatrix = projection * lightView;
	}

	void ShadowMapPass::Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const
	{
		if (context.Settings.Lighting != LightingMode::Forward)
		{
			AddMessage(out, RenderValidationSeverity::Error, "sun shadows need Lighting = Forward");
		}
		if (context.Settings.Depth != DepthMode::Shared)
		{
			AddMessage(out, RenderValidationSeverity::Error, "sun shadows need Depth = Shared");
		}
		if (context.Capabilities.DepthFormat32F == false)
		{
			AddMessage(out, RenderValidationSeverity::Error, "the GPU can't use DEPTH32F as a depth attachment");
		}
	}

	void ShadowMapPass::Setup(RenderGraphBuilder& builder)
	{
		// Read later in the same frame: the hand-written transition at VulkanRenderTarget::End (SampledDepth) synchronises it.
		builder.ImportResource("MeshTextures");
		builder.ImportResource("SpriteTextures");
		AddRecordedPass(builder, PassName, RenderStage::BeforeScene, { "MeshTextures", "SpriteTextures" }, { "SunShadowMap" });
	}

	void ShadowMapPass::Record(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		if (view.SunShadowsActive == false)
		{
			return;
		}

		// Cleared to the far plane: nothing casts. End leaves the map ready to be sampled by the scene pass.
		view.ShadowTarget->Begin(context.CommandBuffer, context.Frame, RenderTargetBeginInfo::Clear(glm::vec4(0.0f)));
		Renderer3D::RecordShadowCasters(context);

		Statistics& stats = ViewRenderer::GetFrameStats();
		const bool hasCasters = std::any_of(view.ShadowCasterDraws.begin(), view.ShadowCasterDraws.end(),
			[](const QuadDraw& draw) { return draw.IndexCount != 0; });
		VulkanPipeline* pipeline = hasCasters ? ViewPipelineCache::Get(ViewPipelineKind::ShadowCaster, *view.ShadowTarget, false) : nullptr;
		if (pipeline != nullptr)
		{
			pipeline->Bind(context.CommandBuffer, view.ShadowTarget->GetExtent());
			QuadBatcher::BindIndexBuffer(context.CommandBuffer);
			uint32_t boundBatch = UINT32_MAX;
			uint32_t boundTextureSet = UINT32_MAX;
			for (const QuadDraw& draw : view.ShadowCasterDraws)
			{
				if (draw.IndexCount == 0)
				{
					continue;
				}
				if (draw.Batch != boundBatch)
				{
					view.QuadVertexBuffers[draw.Batch][context.Frame]->BindAsVertexBuffer(context.CommandBuffer);
					boundBatch = draw.Batch;
				}
				if (draw.TextureSet != boundTextureSet)
				{
					pipeline->BindDescriptorSet(context.CommandBuffer, 0, *view.TextureSets[draw.TextureSet].DescriptorSet);
					boundTextureSet = draw.TextureSet;
				}
				vkCmdDrawIndexed(context.CommandBuffer, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
				stats.DrawCalls++;
			}
		}

		view.ShadowTarget->End(context.CommandBuffer, context.Frame);
		stats.SunShadowViewCount++;
	}
}
