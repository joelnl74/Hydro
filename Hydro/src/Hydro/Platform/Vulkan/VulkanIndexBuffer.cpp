#include "hypch.h"
#include "VulkanIndexBuffer.h"

#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanIndexBuffer::VulkanIndexBuffer(void *sourceData, uint32_t size)
	{
		m_size = size;

		VulkanAllocator allocator("IndexBuffer");

		VkBufferCreateInfo indexbufferCreateInfo = {};
		indexbufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferCreateInfo.size = size;
		indexbufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		m_memory = allocator.AllocateBuffer(indexbufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_Buffer);

		SetData(sourceData, size);
	}

	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t size)
	{
		m_size = size;

		VulkanAllocator allocator("IndexBuffer");

		VkBufferCreateInfo indexbufferCreateInfo = {};
		indexbufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferCreateInfo.size = size;
		indexbufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		m_memory = allocator.AllocateBuffer(indexbufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_Buffer);
	}

	void VulkanIndexBuffer::Bind()
	{
		auto& commandBuffer = Renderer::GetVulkanSwapChain()->GetCommandBuffer();

		vkCmdBindIndexBuffer(commandBuffer, m_Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void VulkanIndexBuffer::Destory()
	{
		VulkanAllocator allocator("IndexBuffer");
		allocator.DestroyBuffer(m_Buffer, m_memory);
	}

	void VulkanIndexBuffer::SetData(void* data, uint32_t size)
	{
		VulkanAllocator allocator("IndexBuffer");
		void* dstBuffer = allocator.MapMemory<void>(m_memory);
		memcpy(dstBuffer, data, size);
		allocator.UnmapMemory(m_memory);
	}

}
