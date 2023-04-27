#pragma once
#include "VulkanAllocator.h"

namespace Hydro
{
	class VulkanIndexBuffer
	{
	public:
		VulkanIndexBuffer(void *data, uint32_t size);
		
		void Bind();
		void Destory();

	private:
		VkBuffer m_Buffer;
		VmaAllocation m_memory;
	};
}
