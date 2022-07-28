#include <hypch.h>

#include "VulkanUtils.h"

#include "VulkanVertexBuffer.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanVertexBuffer::VulkanVertexBuffer(void* sourceData, uint32_t size)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		VkDeviceSize bufferSize = size;
		void* data;

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, sourceData, (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);

		CopyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void VulkanVertexBuffer::Bind()
	{
		auto& commandBuffer = Renderer::GetVulkanPresentation()->GetCommandBuffer();

		VkBuffer vertexBuffers[] = { m_VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	}
}
