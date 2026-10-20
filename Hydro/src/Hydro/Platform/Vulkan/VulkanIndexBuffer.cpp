#include "hypch.h"
#include "VulkanIndexBuffer.h"

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

namespace Hydro
{
	VulkanIndexBuffer::VulkanIndexBuffer(const void* data, uint32_t size)
		: VulkanIndexBuffer(size)
	{
		SetData(data, size);
	}

	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t size)
		: m_Size(size)
	{
		VulkanAllocator allocator("IndexBuffer");

		VkBufferCreateInfo indexBufferCreateInfo = {};
		indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferCreateInfo.size = size;
		indexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		m_Memory = allocator.AllocateBuffer(indexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_Buffer);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		if (m_Buffer == VK_NULL_HANDLE)
		{
			return;
		}

		Renderer::SubmitResourceFree([buffer = m_Buffer, memory = m_Memory]()
		{
			VulkanAllocator allocator("IndexBuffer");
			allocator.DestroyBuffer(buffer, memory);
		});
	}

	void VulkanIndexBuffer::Bind(VkCommandBuffer commandBuffer)
	{
		vkCmdBindIndexBuffer(commandBuffer, m_Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void VulkanIndexBuffer::SetData(const void* data, uint32_t size)
	{
		HY_CORE_ASSERT(size <= m_Size, "VulkanIndexBuffer::SetData: more data than the buffer holds");

		VulkanAllocator allocator("IndexBuffer");
		void* dstBuffer = allocator.MapMemory<void>(m_Memory);
		memcpy(dstBuffer, data, size);
		allocator.UnmapMemory(m_Memory);
	}
}
