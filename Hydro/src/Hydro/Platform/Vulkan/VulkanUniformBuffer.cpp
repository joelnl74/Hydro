#include "hypch.h"
#include "VulkanUniformBuffer.h"

#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		if (device == nullptr && m_uniformBuffers.size() <= 0)
		{
			return;
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
			vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
		}
	}

	void VulkanUniformBuffer::Create(uint32_t size)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDeviceSize bufferSize = (uint32_t)size;

		m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
		m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffers[i], m_uniformBuffersMemory[i]);

			vkMapMemory(device, m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
		}
	}

	void VulkanUniformBuffer::Update(uint32_t currentImage, void *data, uint32_t size)
	{
		void* mappedData = m_uniformBuffersMapped[currentImage];
		memcpy(mappedData, &data, (uint32_t)size);
	}

}

