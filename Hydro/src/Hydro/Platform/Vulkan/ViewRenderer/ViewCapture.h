#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	struct RenderViewData;

	// Saves a view's finished image as a PNG without waiting (RenderView::RequestCapture): the Capture menu, F9 and the
	// before/after baselines. The click-to-select pixel is EntityPicker's.
	class ViewCapture
	{
	public:
		// Right after the view's pass, when its image is readable: copies it if a capture was requested.
		static void Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// After `frame`'s fence: saves the captures that frame index recorded last time.
		static void Resolve(const std::vector<RenderViewData*>& views, uint32_t frame);
	};
}
