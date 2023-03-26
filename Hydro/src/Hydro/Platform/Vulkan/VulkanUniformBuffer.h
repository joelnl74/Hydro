#pragma once
#include "VulkanBuffer.h"

namespace Hydro
{
	class VulkanUniformBuffer : public VulkanBuffer
	{
	public:
		VulkanUniformBuffer() {};
		~VulkanUniformBuffer();

		void Create(unsigned int size);
		void Update(uint32_t currentImage, void* data, uint32_t size);
	private:
		std::vector<VkBuffer> m_uniformBuffers;
		std::vector<VkDeviceMemory> m_uniformBuffersMemory;
		std::vector<void*> m_uniformBuffersMapped;
	};
}


