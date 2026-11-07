#include "hypch.h"
#include "Light2DPass.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/PassTimers.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewRenderer.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptorBuilder.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Platform/Vulkan/VulkanShaderLibrary.h"
#include "Hydro/Platform/Vulkan/VulkanTexture2DData.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	struct Light2DPassData
	{
		Scope<VulkanPipeline> LightPipeline;  // lights added up in the light buffer
		Scope<VulkanPipeline> ShadowPipeline; // blob shadows darkening it; optional on top of the lights
		Scope<VulkanDescriptorBuilder> CompositeLayoutDescriptorSet;
		Scope<VulkanPipeline> CompositePipeline;
	};

	static Light2DPassData* s_Data = nullptr;

	// Lights are drawn as quads through the view's camera set.
	static Scope<VulkanPipeline> BuildLightPipeline(VulkanShader& shader, BlendMode blend)
	{
		PipelineSpecification specification;
		specification.Layout = QuadBatcher::GetVertexLayout();
		specification.DescriptorSets = { &ViewPipelineCache::GetLayoutDescriptorSet() };
		specification.Shader = &shader;
		specification.ColorFormats = { LightBufferFormat };
		specification.Blend = blend;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		return CreateScope<VulkanPipeline>(specification);
	}

	// Set 0 of the composite shader: the light buffer. Only its layout is used; each lit view gets a per-frame set.
	static Scope<VulkanDescriptorBuilder> BuildLightBufferDescriptorSet()
	{
		const VulkanImageInfo& image = GetTexture2DData(*QuadBatcher::GetWhiteTexture()).Image->GetVulkanImageInfo();

		Scope<VulkanDescriptorBuilder> descriptorSet = CreateScope<VulkanDescriptorBuilder>();
		descriptorSet->Begin();
		descriptorSet->BindImage(0, image.ImageView, image.Sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSet->Build();
		return descriptorSet;
	}

	// One triangle over the whole view (no vertex buffer), multiplied over what the scene pass has drawn.
	static Scope<VulkanPipeline> BuildCompositePipeline(VulkanShader& shader)
	{
		PipelineSpecification specification;
		specification.DescriptorSets = { s_Data->CompositeLayoutDescriptorSet.get() };
		specification.Shader = &shader;
		specification.ColorFormats = { ViewColorFormat };
		specification.Blend = BlendMode::Multiply2x;
		specification.BackfaceCulling = false;
		specification.DepthTest = false;
		specification.DepthWrite = false;
		return CreateScope<VulkanPipeline>(specification);
	}

	void Light2DPass::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "Light2DPass::Init called twice");
		s_Data = new Light2DPassData();

		// A missing or broken light shader leaves every view unlit instead of stopping the engine.
		try
		{
			s_Data->LightPipeline = BuildLightPipeline(VulkanShaderLibrary::Load(LightShaderName, "shaders/vertex.vert",
				"shaders/light.frag"),
				BlendMode::Additive);
			s_Data->CompositeLayoutDescriptorSet = BuildLightBufferDescriptorSet();
			s_Data->CompositePipeline = BuildCompositePipeline(VulkanShaderLibrary::Load(CompositeShaderName,
				"shaders/composite.vert",
				"shaders/composite.frag"));
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: 2D lighting is unavailable, views are drawn unlit: {}", e.what());

			s_Data->CompositePipeline.reset();
			s_Data->CompositeLayoutDescriptorSet.reset();
			s_Data->LightPipeline.reset();
		}

		if (s_Data->LightPipeline == nullptr)
		{
			return;
		}

		try
		{
			s_Data->ShadowPipeline = BuildLightPipeline(VulkanShaderLibrary::Load(ShadowShaderName, "shaders/vertex.vert",
				"shaders/shadow.frag"), BlendMode::Darken);
		}
		catch (const std::exception& e)
		{
			HY_CORE_ERROR("Renderer2D: shadows are unavailable, lit views are drawn without them: {}", e.what());

			s_Data->ShadowPipeline.reset();
		}
	}

	void Light2DPass::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		s_Data->ShadowPipeline.reset();
		s_Data->CompositePipeline.reset();
		s_Data->CompositeLayoutDescriptorSet.reset();
		s_Data->LightPipeline.reset();
		delete s_Data;
		s_Data = nullptr;
	}

	VulkanPipeline* Light2DPass::GetCompositePipeline()
	{
		return s_Data->CompositePipeline.get();
	}

	void Light2DPass::OnShaderReloaded(const std::string& name)
	{
		if (name == LightShaderName)
		{
			ViewPipelineCache::RebuildWithShader(s_Data->LightPipeline, LightShaderName);
		}
		else if (name == ShadowShaderName)
		{
			ViewPipelineCache::RebuildWithShader(s_Data->ShadowPipeline, ShadowShaderName);
		}
		else if (name == CompositeShaderName)
		{
			ViewPipelineCache::RebuildWithShader(s_Data->CompositePipeline, CompositeShaderName);
		}
	}

	void Light2DPass::Validate(const RenderGraphContext&, std::vector<RenderValidationMessage>& out) const
	{
		if (s_Data->LightPipeline == nullptr || s_Data->CompositePipeline == nullptr)
		{
			AddMessage(out, RenderValidationSeverity::Warning, "the light shaders are unavailable; views are drawn unlit");
		}
	}

	void Light2DPass::Setup(RenderGraphBuilder& builder)
	{
		builder.ImportResource("SceneColor");
		AddRecordedPass(builder, "LightBuffer", RenderStage::BeforeScene, {}, { "LightBuffer" });
		builder.AddPass({ "LightComposite", RenderStage::AfterLighting, { "LightBuffer" }, { "SceneColor" },
			[this](RenderPassContext& context)
			{
				if (m_DrawnThisFrame)
				{
					RecordComposite(context);
				}
			} });
	}

	void Light2DPass::Record(RenderPassContext& context)
	{
		m_DrawnThisFrame = false;
		VkCommandBuffer commandBuffer = context.CommandBuffer;
		RenderViewData& view = context.View;
		const uint32_t frame = context.Frame;
		const RendererSettings& settings = Renderer::GetSettings();
		if (settings.Lighting == false || view.HasLights == false || s_Data->LightPipeline == nullptr
			|| s_Data->CompositePipeline == nullptr)
		{
			return;
		}

		const uint32_t width = view.Target->GetWidth();
		const uint32_t height = view.Target->GetHeight();

		// Made the first time the view is lit, then kept and resized with the view.
		if (view.LightTarget == nullptr)
		{
			RenderTargetSpecification lightSpecification;
			lightSpecification.Width = width;
			lightSpecification.Height = height;
			lightSpecification.ColorFormats = { LightBufferFormat };
			lightSpecification.DebugName = view.DebugName + " light buffer";
			view.LightTarget = CreateScope<VulkanRenderTarget>(lightSpecification);
			BufferSpecification vertexSpecification;
			vertexSpecification.Usage = BufferUsage::Vertex;
			vertexSpecification.Memory = BufferMemory::HostVisible;
			vertexSpecification.Size = (MaxLightsPerView + MaxShadowsPerView) * 4 * sizeof(QuadVertex);
			vertexSpecification.DebugName = view.DebugName + " lights";
			view.LightVertexBuffers = CreatePerFrameBuffers(vertexSpecification);
		}
		else
		{
			view.LightTarget->Resize(width, height);
		}

		// One quad per light, covering its radius. Texture coordinates run -1..1, which light.frag turns into the distance
		// to the light; the "texture index" carries the number of bands (0 = smooth).
		const float bands = settings.LightFalloff == LightFalloffStyle::Banded ? (float)std::min(std::max(settings.LightBands, 2u), 32u)
			: 0.0f;
		static const glm::vec2 corners[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };

		view.LightVertices.clear();
		for (const LightInstance& light : view.Lights)
		{
			for (const glm::vec2& corner : corners)
			{
				QuadVertex vertex;
				vertex.Position = glm::vec3(light.Position.x + corner.x * light.Radius, light.Position.y + corner.y * light.Radius,
					light.Position.z);
				vertex.Color = glm::vec4(light.Color, 1.0f);
				vertex.TexCoord = corner;
				vertex.TexIndex = bands;
				view.LightVertices.push_back(vertex);
			}
		}

		// The shadows' quads follow the lights' in the same buffer; the colour's alpha is the strength.
		const bool drawShadows = settings.Shadows && s_Data->ShadowPipeline != nullptr;
		if (drawShadows)
		{
			for (const ShadowInstance& shadow : view.Shadows)
			{
				for (const glm::vec2& corner : corners)
				{
					QuadVertex vertex;
					vertex.Position = glm::vec3(shadow.Position.x + corner.x * shadow.Size.x * 0.5f,
						shadow.Position.y + corner.y * shadow.Size.y * 0.5f,
						shadow.Position.z);
					vertex.Color = glm::vec4(0.0f, 0.0f, 0.0f, shadow.Strength);
					vertex.TexCoord = corner;
					vertex.TexIndex = 0.0f;
					view.LightVertices.push_back(vertex);
				}
			}
		}

		const uint32_t lightCount = (uint32_t)view.Lights.size();
		const uint32_t shadowCount = drawShadows ? (uint32_t)view.Shadows.size() : 0;
		if (lightCount + shadowCount > 0)
		{
			view.LightVertexBuffers[frame]->SetData(view.LightVertices.data(), view.LightVertices.size() * sizeof(QuadVertex));
		}

		// A per-frame set, gone when this frame slot is reused, so a resized light buffer needs no rewrite. Every element
		// of the shader's array points at this frame's light image.
		const VulkanImageInfo& lightImage = view.LightTarget->GetImage(frame).GetVulkanImageInfo();
		std::array<VkDescriptorImageInfo, VulkanDescriptorBuilder::MaxImageArraySize> imageInfos{};
		for (VkDescriptorImageInfo& imageInfo : imageInfos)
		{
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = lightImage.ImageView;
			imageInfo.sampler = lightImage.Sampler;
		}
		view.LightBufferSet = VulkanDescriptorAllocator::AllocateFrame(s_Data->CompositeLayoutDescriptorSet->GetDescriptorSetLayout());
		if (view.LightBufferSet == VK_NULL_HANDLE)
		{
			return;
		}
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = view.LightBufferSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = (uint32_t)imageInfos.size();
		write.pImageInfo = imageInfos.data();
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 1, &write, 0, nullptr);

		// Cleared to the ambient light, every point light added on top. End makes it readable by the composite.
		Statistics& stats = ViewRenderer::GetFrameStats();
		PassTimers::BeginLitView();
		PassTimers::WritePhase(commandBuffer, frame, TimerPhase::LightBuffer, false);
		view.LightTarget->Begin(commandBuffer, frame, RenderTargetBeginInfo::Clear(glm::vec4(view.AmbientLight, 1.0f)));
		if (lightCount + shadowCount > 0)
		{
			QuadBatcher::BindIndexBuffer(commandBuffer);
			view.LightVertexBuffers[frame]->BindAsVertexBuffer(commandBuffer);
		}

		if (lightCount > 0)
		{
			s_Data->LightPipeline->Bind(commandBuffer, { width, height });
			// The light shader reads only the camera of the view's first texture set.
			s_Data->LightPipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
			vkCmdDrawIndexed(commandBuffer, lightCount * 6, 1, 0, 0, 0);
			stats.DrawCalls++;
		}

		// Shadows last, so they darken the lights that reach them.
		if (shadowCount > 0)
		{
			PassTimers::WritePhase(commandBuffer, frame, TimerPhase::Shadows, false);
			s_Data->ShadowPipeline->Bind(commandBuffer, { width, height });
			s_Data->ShadowPipeline->BindDescriptorSet(commandBuffer, 0, *view.TextureSets[0].DescriptorSet);
			vkCmdDrawIndexed(commandBuffer, shadowCount * 6, 1, 0, (int32_t)(lightCount * 4), 0);
			stats.DrawCalls++;
			PassTimers::WritePhase(commandBuffer, frame, TimerPhase::Shadows, true);
		}
		view.LightTarget->End(commandBuffer, frame);
		PassTimers::WritePhase(commandBuffer, frame, TimerPhase::LightBuffer, true);

		stats.LitViewCount++;
		stats.LightCount += lightCount;
		stats.ShadowCount += shadowCount;
		m_DrawnThisFrame = true;
	}

	void Light2DPass::RecordComposite(RenderPassContext& context)
	{
		RenderViewData& view = context.View;
		// Built for the view's attachments; it never writes entity IDs.
		VulkanPipeline* pipeline = ViewPipelineCache::Get(ViewPipelineKind::Composite, *view.Target, false, false, view.PassColorTarget);
		if (pipeline == nullptr)
		{
			return;
		}

		PassTimers::WritePhase(context.CommandBuffer, context.Frame, TimerPhase::Composite, false);
		pipeline->Bind(context.CommandBuffer, view.Target->GetExtent());
		pipeline->BindDescriptorSet(context.CommandBuffer, 0, view.LightBufferSet);
		vkCmdDraw(context.CommandBuffer, 3, 1, 0, 0);
		ViewRenderer::GetFrameStats().DrawCalls++;
		PassTimers::WritePhase(context.CommandBuffer, context.Frame, TimerPhase::Composite, true);
	}
}
