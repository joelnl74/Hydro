#pragma once
#include "VulkanBuffer.h"

namespace Hydro
{
	class VulkanUniformBuffer : public VulkanBuffer
	{
	public:
		VulkanUniformBuffer() {};
		~VulkanUniformBuffer();

		void Create(uint32_t size);
		void Update(uint32_t currentImage, void* data, uint32_t size);
		std::vector<VkBuffer>& GetVKBuffers() { return m_uniformBuffers; };
	private:
		std::vector<VkBuffer> m_uniformBuffers;
		std::vector<VkDeviceMemory> m_uniformBuffersMemory;
		std::vector<void*> m_uniformBuffersMapped;
	};
}


