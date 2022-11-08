#include "hypch.h"
#include "VulkanUniformBuffer.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanUniformBuffer::VulkanUniformBuffer(int size)
	{
		auto& context = Renderer::GetRendererContext();
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDeviceSize bufferSize = size;

		m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		m_UniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
		m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			CreateBuffer(
				bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				m_UniformBuffers[i], m_UniformBuffersMemory[i]);

			vkMapMemory(device, m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]);
		}
	}
}
