#include <hypch.h>

#include "VulkanUtils.h"

#include "VulkanVertexBuffer.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

namespace Hydro
{
	VulkanVertexBuffer::VulkanVertexBuffer(const void* data, uint32_t size)
		: VulkanVertexBuffer(size)
	{
		SetData(data, size);
	}

	VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size)
		: m_Size(size)
	{
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = size;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		m_MemoryAllocation = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_VertexBuffer);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		if (m_VertexBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		// A frame in flight may still read it: free it once those frames are done.
		Renderer::SubmitResourceFree([buffer = m_VertexBuffer, allocation = m_MemoryAllocation]()
		{
			VulkanAllocator allocator("VertexBuffer");
			allocator.DestroyBuffer(buffer, allocation);
		});
	}

	void VulkanVertexBuffer::SetData(const void* data, uint32_t size)
	{
		HY_CORE_ASSERT(size <= m_Size, "VulkanVertexBuffer::SetData: more data than the buffer holds");

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
