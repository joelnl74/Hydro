#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	struct RenderViewData;

	// Reads back the entity-ID pixel under a click without waiting (RenderView::RequestEntityIDReadback), for the
	// Editor view's click-to-select. Only the newest request's answer is kept.
	class EntityPicker
	{
	public:
		// Right after the view's pass, when its entity-ID image is readable: copies the requested pixel.
		static void Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame);
		// After `frame`'s fence: takes the picks that frame index recorded last time.
		static void Resolve(const std::vector<RenderViewData*>& views, uint32_t frame);
	};
}
