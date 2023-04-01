#pragma once
#include "VulkanBuffer.h"
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"

namespace Hydro
{
	class VulkanUniformBuffer : public VulkanBuffer
	{
	public:
		VulkanUniformBuffer() {};
		~VulkanUniformBuffer();

		void Create(uint32_t size);
		void Update(const void* data, uint32_t currentImage, uint32_t size);

		std::vector<VkBuffer>& GetVKBuffers() { return m_uniformBuffers; };
	private:
		Ref<VulkanUniformBuffer> instance = nullptr;

		std::vector<VkBuffer> m_uniformBuffers;
		std::vector<VmaAllocation> m_vmaAllocation;
		std::vector<VmaAllocationInfo> allocInfo;
	};
}


