#pragma once
#include "VulkanAllocator.h"

namespace Hydro
{
	class VulkanIndexBuffer
	{
	public:
		VulkanIndexBuffer(void *data, uint32_t size);
		VulkanIndexBuffer(uint32_t size);

		void Bind(VkCommandBuffer commandBuffer);
		void Destroy();
		void SetData(void* data, uint32_t size);

	private:
		VkBuffer m_Buffer;
		VmaAllocation m_memory;
		uint32_t m_size;
	};
}
