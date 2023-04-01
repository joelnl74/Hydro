#pragma once
#include "VulkanAllocator.h"

namespace Hydro
{
	class VulkanIndexBuffer
	{
	public:
		VulkanIndexBuffer(void *data, uint32_t size);
		
		void Bind();

	private:
		VkBuffer m_Buffer;
	};
}
