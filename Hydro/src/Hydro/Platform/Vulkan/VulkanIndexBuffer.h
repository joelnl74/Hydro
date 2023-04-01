#pragma once
#include "VulkanBuffer.h"
#include "VulkanAllocator.h"

namespace Hydro
{
	class VulkanIndexBuffer : public VulkanBuffer
	{
	public:
		VulkanIndexBuffer(void *data, uint32_t size);
		
		void Bind();

	private:
		VkBuffer m_Buffer;
	};
}
