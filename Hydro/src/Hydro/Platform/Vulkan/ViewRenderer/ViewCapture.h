#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	struct RenderViewData;

	// Copies a view's finished images back to the CPU without waiting: PNG captures (RenderView::RequestCapture) and
	// the entity-ID pixel under a click (RenderView::RequestEntityIDReadback).
	class ViewCapture
	{
	public:
		// Right after the view's pass, when its images are readable: copies what was requested.
		static void Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// After `frame`'s fence: saves the captures and takes the picks that frame index recorded last time.
		static void Resolve(const std::vector<RenderViewData*>& views, uint32_t frame);
	};
}
