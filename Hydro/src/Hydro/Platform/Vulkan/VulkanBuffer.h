#pragma once
#include "VulkanUtils.h"
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanBuffer
	{
	public:
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	};
}
