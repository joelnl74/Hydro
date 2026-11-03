#include "hypch.h"
#include "ViewRenderer.h"

#include "Hydro/Platform/Vulkan/ViewRenderer/EntityPicker.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/PassTimers.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/QuadBatcher.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/SceneLightingBuffer.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewCapture.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/ViewPipelineCache.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/Light2DPass.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/MeshPass.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/QuadPasses.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/ShadowMapPass.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/PostProcess/BloomPass.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/PostProcess/DepthOfFieldPass.h"
#include "Hydro/Platform/Vulkan/ViewRenderer/Passes/PostProcess/TonemapPass.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	static constexpr ImageFormat SharedDepthFormat = ImageFormat::DEPTH32F;

	struct ViewRendererData
	{
		std::vector<RenderViewData*> Views;       // every RenderView that exists
		std::vector<RenderViewData*> QueuedViews; // begun this frame, in BeginScene order

		Statistics Stats;
		Statistics LastFrameStats;
	};

	static ViewRendererData* s_Data = nullptr;

	// The Editor view's entity-ID attachment (R32_SINT, cleared to -1): an entity's sprites and meshes write their ID
	// wherever they draw a pixel, for click-to-select.
	class EntityIDFeature : public RenderFeature
	{
	public:
		const char* GetName() const override { return "EntityID"; }

		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override
		{
			if (context.Capabilities.IndependentBlend == false)
			{
				AddMessage(out, RenderValidationSeverity::Error, "the GPU has no independentBlend; the Editor view has no entity IDs");
			}
		}

		void Setup(RenderGraphBuilder& builder) override
		{
			builder.RequireEntityIDAttachment();
			builder.ImportResource("EntityID");
		}
	};

	// One DEPTH32F attachment, cleared when the view's pass begins and kept through every later stage, so meshes,
	// cut-out sprites, decals and translucent quads test against each other.
	class SharedDepthFeature : public RenderFeature
	{
	public:
		const char* GetName() const override { return "Depth"; }

		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override
		{
			if (context.Capabilities.DepthFormat32F == false)
			{
				AddMessage(out, RenderValidationSeverity::Error, "the GPU can't use DEPTH32F as a depth attachment");
			}
		}

		void Setup(RenderGraphBuilder& builder) override
		{
			builder.RequireDepthAttachment(SharedDepthFormat);
			builder.ImportResource("Depth");
		}
	};

	// No passes of its own: the view's set 0 carries its lights every frame and the sprite and mesh passes use their
	// lit shaders.
	class ForwardLightingFeature : public RenderFeature
	{
	public:
		const char* GetName() const override { return "ForwardLighting"; }

		void Validate(const RenderGraphContext& context, std::vector<RenderValidationMessage>& out) const override
		{
			if (context.Settings.Depth != DepthMode::Shared)
			{
				AddMessage(out, RenderValidationSeverity::Warning,
					"forward lighting without shared depth: sprites are lit, but sort in painter's order");
			}
		}

		void Setup(RenderGraphBuilder&) override {}
	};

	// In the order their passes run within a stage. `editorView`: the Editor window's view.
	static std::vector<Scope<RenderFeature>> CreateRenderFeatures(const RenderFeatureSettings& settings, bool editorView)
	{
		std::vector<Scope<RenderFeature>> features;
		const bool sharedDepth = Renderer2D::HasSharedDepth(settings);

		if (editorView)
		{
			features.push_back(CreateScope<EntityIDFeature>());
		}
		if (settings.Lighting == LightingMode::LightBuffer2D)
		{
			features.push_back(CreateScope<Light2DPass>());
		}
		if (settings.Depth == DepthMode::Shared)
		{
			features.push_back(CreateScope<SharedDepthFeature>());
		}
		if (settings.Meshes)
		{
			features.push_back(CreateScope<MeshPass>(editorView));
		}
		if (sharedDepth)
		{
			features.push_back(CreateScope<DecalPass>(editorView));
			features.push_back(CreateScope<CutoutSpritePass>(editorView));
		}
		features.push_back(CreateScope<TranslucentPass>(sharedDepth, editorView));
		features.push_back(CreateScope<OverlayPass>());
		if (settings.Lighting == LightingMode::Forward)
		{
			features.push_back(CreateScope<ForwardLightingFeature>());
		}
		if (settings.Shadows == ShadowMode::Sun)
		{
			features.push_back(CreateScope<ShadowMapPass>());
		}

		if (settings.HDR)
		{
			if (settings.Bloom)
			{
				features.push_back(CreateScope<BloomPass>(true));
			}
			if (settings.DepthOfField)
			{
				features.push_back(CreateScope<DepthOfFieldPass>(true));
			}
			const bool depthOfField = settings.DepthOfField && settings.Depth == DepthMode::Shared;
			features.push_back(CreateScope<TonemapPass>(settings.Bloom, depthOfField));
		}
		else
		{
			// Only to report that they need HDR.
			if (settings.Bloom)
			{
				features.push_back(CreateScope<BloomPass>(false));
			}
			if (settings.DepthOfField)
			{
				features.push_back(CreateScope<DepthOfFieldPass>(false));
			}
		}

		if (editorView)
		{
			features.push_back(CreateScope<DebugLinePass>(sharedDepth));
		}
		return features;
	}

	void ViewRenderer::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "ViewRenderer::Init called twice");
		s_Data = new ViewRendererData();

		// In dependency order: view descriptor sets need the white texture and the empty shadow map, the light pipelines
		// the cache's set 0 layout.
		QuadBatcher::Init();
		ShadowMapPass::Init();
		ViewPipelineCache::Init();
		DepthOfFieldPass::Init();
		Light2DPass::Init();
		PassTimers::Init();
	}

	void ViewRenderer::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		// Views belong to the application and must be gone by now (layers release them in OnDetach).
		HY_CORE_ASSERT(s_Data->Views.empty(), "A RenderView still exists at Renderer2D::Shutdown");

		// Resources free themselves through the deletion queue, which Renderer::Shutdown flushes next.
		PassTimers::Shutdown();
		Light2DPass::Shutdown();
		DepthOfFieldPass::Shutdown();
		ViewPipelineCache::Shutdown();
		ShadowMapPass::Shutdown();
		QuadBatcher::Shutdown();

		delete s_Data;
		s_Data = nullptr;
	}

	void ViewRenderer::RegisterView(RenderViewData& view)
	{
		HY_CORE_ASSERT(s_Data != nullptr, "Create RenderViews after Renderer::Init");
		QuadBatcher::AddView(view);
		s_Data->Views.push_back(&view);
	}

	void ViewRenderer::UnregisterView(RenderViewData& view)
	{
		if (s_Data == nullptr)
		{
			return;
		}

		auto& views = s_Data->Views;
		views.erase(std::remove(views.begin(), views.end(), &view), views.end());
		auto& queued = s_Data->QueuedViews;
		queued.erase(std::remove(queued.begin(), queued.end(), &view), queued.end());
	}

	void ViewRenderer::QueueView(RenderViewData& view)
	{
		if (view.Queued == false)
		{
			view.Queued = true;
			s_Data->QueuedViews.push_back(&view);
		}
	}

	void ViewRenderer::Render(VkCommandBuffer commandBuffer)
	{
		ViewPipelineCache::ApplySettings();

		// Renderer::RenderFrame waited on this frame's fence: the GPU is done with every view's buffers, descriptor sets,
		// copies and timestamps for `frame`, so they can be read and overwritten.
		const uint32_t frame = Renderer::GetRenderFrame();
		UpdateImGuiTextures(frame);
		ViewCapture::Resolve(s_Data->Views, frame);
		EntityPicker::Resolve(s_Data->Views, frame);
		PassTimers::BeginFrame(commandBuffer, frame);

		for (RenderViewData* view : s_Data->QueuedViews)
		{
			RecordView(commandBuffer, *view, frame);
			s_Data->Stats.ViewCount++;
		}

		PassTimers::EndFrame(commandBuffer, frame);

		Statistics& stats = s_Data->Stats;
		GPUMemoryStats memory = VulkanAllocator::GetStats();
		stats.GPUMemoryUsed = memory.Used;
		stats.GPUMemoryFree = memory.Free;
		stats.MaxQuadsPerBatch = QuadBatcher::MaxQuads;
		stats.MaxTextureSlots = MaxTextureSlots;
		stats.FramesInFlight = MAX_FRAMES_IN_FLIGHT;
		stats.FrameIndex = frame;
		PassTimers::FillStats(stats);
		if (s_Data->QueuedViews.empty() == false)
		{
			stats.ViewportWidth = s_Data->QueuedViews.front()->Target->GetWidth();
			stats.ViewportHeight = s_Data->QueuedViews.front()->Target->GetHeight();
		}

		s_Data->LastFrameStats = stats;
		DiscardFrame();
	}

	void ViewRenderer::DiscardFrame()
	{
		for (RenderViewData* view : s_Data->QueuedViews)
		{
			view->Queued = false;
			QuadBatcher::Reset(*view);
		}
		s_Data->QueuedViews.clear();
		s_Data->Stats = {};
	}

	Statistics& ViewRenderer::GetFrameStats()
	{
		return s_Data->Stats;
	}

	const Statistics& ViewRenderer::GetLastFrameStats()
	{
		return s_Data->LastFrameStats;
	}

	void ViewRenderer::RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		ApplyViewResize(view);

		// Before the uploads: what the graph holds decides whether the view draws a sun shadow map this frame.
		UpdateRenderGraph(view);
		ShadowMapPass::Prepare(view);

		UniformBufferObject ubo;
		ubo.mvp = view.ViewProjection;
		view.UniformBuffers[frame]->SetData(&ubo, sizeof(UniformBufferObject));
		SceneLightingBuffer::Upload(view, frame);
		QuadBatcher::Upload(view, frame);

		RenderPassContext context{ commandBuffer, frame, view };
		PassTimers::BeginLabel(commandBuffer, ("View " + view.DebugName).c_str());

		// Into their own targets, before the view's target is begun.
		ExecuteStage(RenderStage::BeforeScene, context);

		const RenderTargetBeginInfo clear = RenderTargetBeginInfo::Clear(Renderer::GetSettings().ClearColor);
		if (view.SceneColor != nullptr && view.Graph->HasPass(TonemapPass::PassName))
		{
			// HDR: the scene pass draws into the scene colour with the view's depth and IDs, PostProcess runs outside any
			// pass, then the output pass loads depth and IDs again for the tonemap, the overlay and the debug lines.
			view.PassColorTarget = view.SceneColor.get();
			view.Target->Begin(commandBuffer, frame, clear, view.SceneColor.get());
			for (uint8_t stage = (uint8_t)RenderStage::Opaque; stage <= (uint8_t)RenderStage::AfterLighting; stage++)
			{
				ExecuteStage((RenderStage)stage, context);
			}
			view.Target->End(commandBuffer, frame, view.SceneColor.get());
			view.PassColorTarget = nullptr;

			ExecuteStage(RenderStage::PostProcess, context);

			RenderTargetBeginInfo output;
			output.ColorLoad = AttachmentLoadOp::DontCare; // the tonemap writes every pixel
			output.DepthLoad = AttachmentLoadOp::Load;
			output.EntityIDLoad = AttachmentLoadOp::Load;
			view.Target->Begin(commandBuffer, frame, output);
			for (uint8_t stage = (uint8_t)RenderStage::Resolve; stage < (uint8_t)RenderStage::Count; stage++)
			{
				ExecuteStage((RenderStage)stage, context);
			}
			view.Target->End(commandBuffer, frame);
		}
		else
		{
			// One rendering pass, so the target is cleared exactly once. End moves the image to SHADER_READ_ONLY_OPTIMAL
			// for ImGui, which samples it later in this command buffer.
			view.Target->Begin(commandBuffer, frame, clear);
			for (uint8_t stage = (uint8_t)RenderStage::Opaque; stage < (uint8_t)RenderStage::Count; stage++)
			{
				if ((RenderStage)stage != RenderStage::PostProcess)
				{
					ExecuteStage((RenderStage)stage, context);
				}
			}
			view.Target->End(commandBuffer, frame);
		}

		s_Data->Stats.BatchesAllocated += (uint32_t)view.QuadVertexBufferBases.size();

		// After the pass, so a capture has everything the view drew, overlay included.
		ViewCapture::Record(commandBuffer, view, frame);
		EntityPicker::Record(commandBuffer, view, frame);
		PassTimers::EndLabel(commandBuffer);
	}

	void ViewRenderer::ExecuteStage(RenderStage stage, RenderPassContext& context)
	{
		for (const RenderPassDeclaration& pass : context.View.Graph->GetPasses())
		{
			if (pass.Stage != stage)
			{
				continue;
			}

			PassTimers::BeginLabel(context.CommandBuffer, pass.Name.c_str());
			const uint32_t timer = PassTimers::BeginPass(context.CommandBuffer, context.Frame, pass.Name);
			pass.Execute(context);
			PassTimers::EndPass(context.CommandBuffer, context.Frame, timer);
			PassTimers::EndLabel(context.CommandBuffer);
		}
	}

	std::vector<RenderValidationMessage> ViewRenderer::ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView)
	{
		RenderGraphContext context;
		context.Settings = settings;
		context.Capabilities = Renderer::GetCapabilities();
		context.EditorView = editorView;
		return RenderGraph::Validate(CreateRenderFeatures(settings, editorView), context);
	}

	void ViewRenderer::UpdateRenderGraph(RenderViewData& view)
	{
		const bool upToDate = view.Graph != nullptr && view.Graph->GetContext().Settings == view.Features
			&& view.Graph->GetContext().EditorView == view.EditorView;
		if (upToDate)
		{
			return;
		}

		// CPU only: the old graph holds no GPU objects, so it can go at once.
		RenderGraphContext context;
		context.Settings = view.Features;
		context.Capabilities = Renderer::GetCapabilities();
		context.EditorView = view.EditorView;
		view.Graph = CreateScope<RenderGraph>(CreateRenderFeatures(view.Features, view.EditorView), context, view.DebugName);

		// The scene colour is made or dropped with the HDR feature (old images through the deletion queue).
		const ImageFormat sceneColorFormat = view.Graph->GetSceneColorFormat();
		if (sceneColorFormat == ImageFormat::None)
		{
			view.SceneColor.reset();
			view.BloomLevels.clear(); // no HDR, no bloom: free the chain too (#157)
		}
		if (view.Graph->HasPass(DepthOfFieldPass::PassName) == false)
		{
			DepthOfFieldPass::Release(view);
		}
		else if (view.SceneColor == nullptr || view.SceneColor->GetSpecification().ColorFormats[0] != sceneColorFormat)
		{
			RenderTargetSpecification specification;
			specification.Width = view.Target->GetWidth();
			specification.Height = view.Target->GetHeight();
			specification.ColorFormats = { sceneColorFormat };
			specification.DebugName = view.DebugName + " scene colour";
			view.SceneColor = CreateScope<VulkanRenderTarget>(specification);
		}

		// A new target when the features need other attachments (depth, a readable depth, entity ID).
		const RenderTargetSpecification& current = view.Target->GetSpecification();
		if (view.Graph->GetDepthFormat() != current.DepthFormat || view.Graph->NeedsEntityID() != current.EntityID
			|| view.Graph->NeedsSampledDepth() != current.SampledDepth)
		{
			RenderTargetSpecification specification = current;
			specification.DepthFormat = view.Graph->GetDepthFormat();
			specification.EntityID = view.Graph->NeedsEntityID();
			specification.SampledDepth = view.Graph->NeedsSampledDepth();
			view.Target = CreateScope<VulkanRenderTarget>(specification);
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				view.ImGuiTexturesOutdated[i] = true;
			}
			UpdateImGuiTextures(view, Renderer::GetRenderFrame());
		}
	}

	void ViewRenderer::ApplyViewResize(RenderViewData& view)
	{
		const uint32_t width = view.RequestedWidth;
		const uint32_t height = view.RequestedHeight;
		if (width == view.Target->GetWidth() && height == view.Target->GetHeight())
		{
			return;
		}

		// The old images go through the deletion queue: the other frame in flight may still use them.
		view.Target->Resize(width, height);
		if (view.SceneColor != nullptr)
		{
			view.SceneColor->Resize(width, height);
		}
		// A click made on the old size points at another pixel now.
		view.PickGeneration++;

		// Each frame index's ImGui handle still points at that index's old image, which the frame in flight may be drawing
		// with: it is rewritten when that index is recorded next (this frame's now), so no sets leak from ImGui's pool.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			view.ImGuiTexturesOutdated[i] = true;
		}
		UpdateImGuiTextures(view, Renderer::GetRenderFrame());
	}

	void ViewRenderer::UpdateImGuiTextures(uint32_t frame)
	{
		// Every view, not just the ones begun this frame: ImGui may show a view that isn't drawn.
		for (RenderViewData* view : s_Data->Views)
		{
			UpdateImGuiTextures(*view, frame);
		}
	}

	void ViewRenderer::UpdateImGuiTextures(RenderViewData& view, uint32_t frame)
	{
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

	void ViewRenderer::WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler)
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.sampler = sampler;
		imageInfo.imageView = view;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), 1, &write, 0, nullptr);
	}
}
