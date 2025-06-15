#include <hypch.h>

#include "VulkanUtils.h"

#include "VulkanVertexBuffer.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanVertexBuffer::VulkanVertexBuffer(void* sourceData, uint32_t size)
	{
		auto &device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_MemoryAllocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_VertexBuffer);
		SetData(sourceData, size);
	}

	VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_MemoryAllocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_VertexBuffer);
	}

	void VulkanVertexBuffer::Destroy()
	{
		VulkanAllocator allocator("VertexBuffer");
		allocator.DestroyBuffer(m_VertexBuffer, m_MemoryAllocation);
	}

	void VulkanVertexBuffer::SetData(void* data, uint32_t size)
	{
		VulkanAllocator allocator("VertexBuffer");
		void* dstBuffer = allocator.MapMemory<void>(m_MemoryAllocation);
		memcpy(dstBuffer, data, size);
		allocator.UnmapMemory(m_MemoryAllocation);
	}

	void VulkanVertexBuffer::Bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer vertexBuffers[] = { m_VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	}
}
