#pragma once
#include <vk_mem_alloc.h>

#include <vector>

namespace Hydro
{
	class VulkanUniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size);
		~VulkanUniformBuffer();

		void Update(const void* data, uint32_t currentImage, uint32_t size);
		void Destroy();

		std::vector<VkBuffer>& GetVKBuffers() { return m_uniformBuffers; };
	private:
		std::vector<VkBuffer> m_uniformBuffers;
		std::vector<VmaAllocation> m_vmaAllocation;
	};
}


