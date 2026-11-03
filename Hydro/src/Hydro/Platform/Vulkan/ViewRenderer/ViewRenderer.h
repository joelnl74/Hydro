#pragma once
#include "Hydro/Renderer/Renderer2D.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	struct RenderViewData;

	// Renders the RenderViews begun this frame, each through its render graph between its target's passes, and keeps
	// every view's graph, attachments and ImGui handles up to date. Starts and stops the services and passes it uses.
	class ViewRenderer
	{
	public:
		static void Init();
		static void Shutdown();

		static void RegisterView(RenderViewData& view);
		static void UnregisterView(RenderViewData& view);
		// Adds `view` to this frame's list, once.
		static void QueueView(RenderViewData& view);

		// Records every queued view into the frame's command buffer, after its fence, then starts the next frame.
		static void Render(VkCommandBuffer commandBuffer);
		// Drops this frame's quads without rendering (the window is minimized).
		static void DiscardFrame();

		// This frame's statistics, while they are counted, and the last rendered frame's.
		static Statistics& GetFrameStats();
		static const Statistics& GetLastFrameStats();

		static std::vector<RenderValidationMessage> ValidateRenderFeatures(const RenderFeatureSettings& settings, bool editorView);
		// Binding 0, combined image sampler: the layout ImGui_ImplVulkan_AddTexture uses.
		static void WriteImGuiTexture(VkDescriptorSet set, VkImageView view, VkSampler sampler);

	private:
		static void RecordView(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// Each pass in a debug label, timed while the frame's timers last.
		static void ExecuteStage(RenderStage stage, RenderPassContext& context);
		// Rebuilds the view's graph when its features changed, and its target when they need other attachments.
		static void UpdateRenderGraph(RenderViewData& view);
		static void ApplyViewResize(RenderViewData& view);
		// Points the ImGui handles of `frame` that a resize left outdated at the views' new images. Only after `frame`'s
		// fence and before the present pass is recorded.
		static void UpdateImGuiTextures(uint32_t frame);
		static void UpdateImGuiTextures(RenderViewData& view, uint32_t frame);
	};
}
