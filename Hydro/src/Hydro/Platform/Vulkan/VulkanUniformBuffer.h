#pragma once
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"

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
		Ref<VulkanUniformBuffer> instance = nullptr;

		std::vector<VkBuffer> m_uniformBuffers;
		std::vector<VmaAllocation> m_vmaAllocation;
		std::vector<VmaAllocationInfo> allocInfo;
	};
}


