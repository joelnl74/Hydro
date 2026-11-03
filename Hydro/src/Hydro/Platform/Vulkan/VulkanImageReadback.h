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

	// Copies a region of a colour image to host memory without waiting: Record adds the copy to a frame, TakeResult
	// hands the pixels over MAX_FRAMES_IN_FLIGHT frames later. Used for view captures (RenderView::RequestCapture).
	class VulkanImageReadback
	{
	public:
		VulkanImageReadback() = default;
		~VulkanImageReadback();

		VulkanImageReadback(const VulkanImageReadback&) = delete;
		VulkanImageReadback& operator=(const VulkanImageReadback&) = delete;

		// Copies the region into `frame`'s host buffer; `layout` is the image's layout at this point and is restored.
		// Size 0 = the whole image. False (nothing recorded) when the slot is still full or the format unsupported.
		bool Record(VkCommandBuffer commandBuffer, const VulkanImage& image, VkImageLayout layout, uint32_t frame,
			uint32_t x = 0, uint32_t y = 0, uint32_t width = 0, uint32_t height = 0);

		// Only after `frame`'s fence has signalled: moves the pixels into `out` and frees the slot.
		bool TakeResult(uint32_t frame, ImageReadbackResult& out);

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
