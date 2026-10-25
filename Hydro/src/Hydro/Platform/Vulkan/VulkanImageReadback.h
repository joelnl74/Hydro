#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VulkanFrameContext.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Hydro
{
	class VulkanImage;

	// Pixels copied out of a GPU image, rows top to bottom, tightly packed.
	struct ImageReadbackResult
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		VkFormat Format = VK_FORMAT_UNDEFINED;
		uint32_t BytesPerPixel = 0;
		std::vector<uint8_t> Pixels;
	};

	// Copies a region of a colour image into host memory without waiting for the GPU. Record() adds the
	// copy to the frame's command buffer; TakeResult() hands the pixels over once that frame's fence has
	// signalled (the next time the same frame index is recorded), so the answer arrives
	// MAX_FRAMES_IN_FLIGHT frames later. One slot per frame in flight; its host buffer is kept and
	// reused, and grows when a bigger region is asked for.
	//
	// A resource: non-copyable, hold it in a Scope. The buffers are freed through
	// Renderer::SubmitResourceFree, so it can be destroyed while a copy is still in flight.
	// Used for view captures (RenderView::RequestCapture); entity-ID picking will use it too.
	class VulkanImageReadback
	{
	public:
		VulkanImageReadback() = default;
		~VulkanImageReadback();

		VulkanImageReadback(const VulkanImageReadback&) = delete;
		VulkanImageReadback& operator=(const VulkanImageReadback&) = delete;

		// Records into `commandBuffer`: `image` goes from `layout` to TRANSFER_SRC_OPTIMAL, the region is
		// copied into `frame`'s host buffer, and the image goes back to `layout`. `layout` must be the
		// image's layout at this point of the command buffer; everything written to it before is
		// included. A width or height of 0 means the whole image (the region is clamped to the image).
		// Returns false, recording nothing, when `frame`'s slot still holds a result nobody took, the
		// format is unsupported (depth, compressed) or the buffer can't be allocated.
		bool Record(VkCommandBuffer commandBuffer, const VulkanImage& image, VkImageLayout layout, uint32_t frame,
			uint32_t x = 0, uint32_t y = 0, uint32_t width = 0, uint32_t height = 0);

		// Call only after `frame`'s fence has signalled. Moves the copied pixels into `out` and frees the
		// slot. Returns false when nothing was recorded for `frame`.
		bool TakeResult(uint32_t frame, ImageReadbackResult& out);

		// True while `frame`'s slot holds a copy that hasn't been taken.
		bool IsPending(uint32_t frame) const { return m_Slots[frame].Pending; }
		bool IsAnyPending() const;

		// Bytes per pixel of the formats this can read back; 0 for the rest.
		static uint32_t GetBytesPerPixel(VkFormat format);

	private:
		struct Slot
		{
			VkBuffer Buffer = VK_NULL_HANDLE;
			VmaAllocation Allocation = nullptr;
			VkDeviceSize Capacity = 0;
			uint32_t Width = 0;
			uint32_t Height = 0;
			VkFormat Format = VK_FORMAT_UNDEFINED;
			uint32_t BytesPerPixel = 0;
			bool Pending = false;
		};

		std::array<Slot, MAX_FRAMES_IN_FLIGHT> m_Slots;
	};
}
