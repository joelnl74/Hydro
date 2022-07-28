#pragma once
#include "VulkanBuffer.h"

namespace Hydro
{
	class VulkanIndexBuffer : public VulkanBuffer
	{
	public:
		VulkanIndexBuffer(void *data, uint32_t size);
		
		void Bind();

	private:
		VkBuffer indexBuffer;
		VkDeviceMemory indexBufferMemory;
	};
}
