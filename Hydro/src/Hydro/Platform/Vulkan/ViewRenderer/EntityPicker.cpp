#include "hypch.h"
#include "EntityPicker.h"

#include "Hydro/Platform/Vulkan/VulkanImageReadback.h"
#include "Hydro/Platform/Vulkan/VulkanRenderTarget.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"

#include <cstring>

namespace Hydro
{
	// The answer only when it belongs to the newest request and nothing invalidated it since (a resize, a scene change).
	static void ResolvePick(RenderViewData& view, uint32_t frame)
	{
		if (view.PickReadback == nullptr || view.PickReadback->IsPending(frame) == false)
		{
			return;
		}

		ImageReadbackResult result;
		if (view.PickReadback->TakeResult(frame, result) == false || result.Pixels.size() < sizeof(int32_t))
		{
			return;
		}
		if (view.PickSlotGeneration[frame] != view.PickGeneration || view.PickSlotRequest[frame] != view.PickRequest)
		{
			return;
		}

		int32_t id = -1;
		memcpy(&id, result.Pixels.data(), sizeof(int32_t));
		view.PickResult = id;
		view.PickResultReady = true;
	}

	static void RecordPick(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		if (view.PickRequested == false)
		{
			return;
		}

		const VulkanImage* ids = view.Target->GetEntityIDImage(frame);
		if (ids == nullptr || view.PickX >= view.Target->GetWidth() || view.PickY >= view.Target->GetHeight())
		{
			// No IDs in this view, or the click is outside the (resized) image: nothing is selected.
			view.PickRequested = false;
			return;
		}

		if (view.PickReadback == nullptr)
		{
			view.PickReadback = CreateScope<VulkanImageReadback>();
		}
		// This frame index's last copy hasn't been read yet: try again next frame.
		if (view.PickReadback->IsPending(frame))
		{
			return;
		}

		if (view.PickReadback->Record(commandBuffer, *ids, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frame, view.PickX, view.PickY, 1, 1))
		{
			view.PickSlotRequest[frame] = view.PickRequest;
			view.PickSlotGeneration[frame] = view.PickGeneration;
		}
		view.PickRequested = false;
	}

	void EntityPicker::Resolve(const std::vector<RenderViewData*>& views, uint32_t frame)
	{
		// Every view, not only the ones begun this frame: a copy may belong to a view that is hidden now.
		for (RenderViewData* view : views)
		{
			ResolvePick(*view, frame);
		}
	}

	void EntityPicker::Record(VkCommandBuffer commandBuffer, RenderViewData& view, uint32_t frame)
	{
		RecordPick(commandBuffer, view, frame);
	}
}
