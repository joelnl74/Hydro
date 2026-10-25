#include "hypch.h"
#include "VulkanImageReadback.h"

#include "VulkanAllocator.h"
#include "VulkanImage.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanImageReadback::~VulkanImageReadback()
	{
		std::vector<std::pair<VkBuffer, VmaAllocation>> buffers;
		for (const Slot& slot : m_Slots)
		{
			if (slot.Buffer != VK_NULL_HANDLE)
			{
				buffers.emplace_back(slot.Buffer, slot.Allocation);
			}
		}

		if (buffers.empty())
		{
			return;
		}

		// A copy may still be in flight: free after the frames that could be writing them.
		Renderer::SubmitResourceFree([buffers]()
		{
			VulkanAllocator allocator("ImageReadback");
			for (const auto& [buffer, allocation] : buffers)
			{
				allocator.DestroyBuffer(buffer, allocation);
			}
		});
	}

	uint32_t VulkanImageReadback::GetBytesPerPixel(VkFormat format)
	{
		switch (format)
		{
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_R32_SINT:
			case VK_FORMAT_R32_UINT:
			case VK_FORMAT_R32_SFLOAT:
				return 4;
			case VK_FORMAT_R16G16B16A16_SFLOAT:
				return 8;
			case VK_FORMAT_R32G32B32A32_SFLOAT:
				return 16;
			default:
				return 0;
		}
	}

	bool VulkanImageReadback::IsAnyPending() const
	{
		for (const Slot& slot : m_Slots)
		{
			if (slot.Pending)
			{
				return true;
			}
		}

		return false;
	}

	bool VulkanImageReadback::Record(VkCommandBuffer commandBuffer, const VulkanImage& image, VkImageLayout layout, uint32_t frame,
		uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		HY_CORE_ASSERT(frame < MAX_FRAMES_IN_FLIGHT, "VulkanImageReadback::Record: frame index out of range");

		Slot& slot = m_Slots[frame];
		if (slot.Pending)
		{
			return false;
		}

		const uint32_t bytesPerPixel = GetBytesPerPixel(image.GetFormat());
		if (bytesPerPixel == 0)
		{
			HY_CORE_ERROR("VulkanImageReadback: format {} can't be read back", (int)image.GetFormat());
			return false;
		}

		if (x >= image.GetWidth() || y >= image.GetHeight())
		{
			return false;
		}

		if (width == 0 || x + width > image.GetWidth())
		{
			width = image.GetWidth() - x;
		}
		if (height == 0 || y + height > image.GetHeight())
		{
			height = image.GetHeight() - y;
		}

		// Grow the host buffer when needed. The old buffer's last copy was taken (the slot isn't
		// pending), but it goes through the deletion queue like every GPU object.
		const VkDeviceSize size = (VkDeviceSize)width * height * bytesPerPixel;
		if (slot.Capacity < size)
		{
			if (slot.Buffer != VK_NULL_HANDLE)
			{
				Renderer::SubmitResourceFree([buffer = slot.Buffer, allocation = slot.Allocation]()
				{
					VulkanAllocator("ImageReadback").DestroyBuffer(buffer, allocation);
				});
				slot.Buffer = VK_NULL_HANDLE;
				slot.Allocation = nullptr;
				slot.Capacity = 0;
			}

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VulkanAllocator allocator("ImageReadback");
			slot.Allocation = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_GPU_TO_CPU, slot.Buffer);
			if (slot.Buffer == VK_NULL_HANDLE)
			{
				HY_CORE_ERROR("VulkanImageReadback: can't allocate a {} byte buffer", (uint64_t)size);
				slot.Allocation = nullptr;
				return false;
			}
			slot.Capacity = size;
		}

		const VkImage vkImage = image.GetVulkanImageInfo().Image;

		VkImageMemoryBarrier toTransfer = {};
		toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toTransfer.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		toTransfer.oldLayout = layout;
		toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = vkImage;
		toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		// Rarely used (a capture, a click), so the broad stage masks cost nothing that matters and cover
		// whatever wrote the image before (a render pass, a transfer).
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &toTransfer);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;   // tightly packed
		region.bufferImageHeight = 0;
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageOffset = { (int32_t)x, (int32_t)y, 0 };
		region.imageExtent = { width, height, 1 };
		vkCmdCopyImageToBuffer(commandBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, slot.Buffer, 1, &region);

		VkImageMemoryBarrier back = toTransfer;
		back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		back.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		back.newLayout = layout;

		// The copy's writes must be visible to the host once the fence has signalled.
		VkBufferMemoryBarrier toHost = {};
		toHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toHost.buffer = slot.Buffer;
		toHost.offset = 0;
		toHost.size = size;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
			0, 0, nullptr, 1, &toHost, 1, &back);

		slot.Width = width;
		slot.Height = height;
		slot.Format = image.GetFormat();
		slot.BytesPerPixel = bytesPerPixel;
		slot.Pending = true;
		return true;
	}

	bool VulkanImageReadback::TakeResult(uint32_t frame, ImageReadbackResult& out)
	{
		HY_CORE_ASSERT(frame < MAX_FRAMES_IN_FLIGHT, "VulkanImageReadback::TakeResult: frame index out of range");

		Slot& slot = m_Slots[frame];
		if (slot.Pending == false)
		{
			return false;
		}

		slot.Pending = false;

		const size_t size = (size_t)slot.Width * slot.Height * slot.BytesPerPixel;
		VmaAllocator vma = VulkanAllocator::GetVMAAllocator();
		// GPU_TO_CPU memory may be non-coherent: make the GPU's writes visible before reading.
		vmaInvalidateAllocation(vma, slot.Allocation, 0, VK_WHOLE_SIZE);

		VulkanAllocator allocator("ImageReadback");
		const uint8_t* mapped = allocator.MapMemory<uint8_t>(slot.Allocation);
		if (mapped == nullptr)
		{
			HY_CORE_ERROR("VulkanImageReadback: can't map the readback buffer");
			return false;
		}

		out.Width = slot.Width;
		out.Height = slot.Height;
		out.Format = slot.Format;
		out.BytesPerPixel = slot.BytesPerPixel;
		out.Pixels.assign(mapped, mapped + size);
		allocator.UnmapMemory(slot.Allocation);
		return true;
	}
}
