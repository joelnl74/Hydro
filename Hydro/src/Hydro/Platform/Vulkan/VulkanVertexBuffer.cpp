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

		auto bufferAlloc = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_VertexBuffer);

		void* dstBuffer = allocator.MapMemory<void>(bufferAlloc);
		memcpy(dstBuffer, sourceData, size);
		allocator.UnmapMemory(bufferAlloc);
	}

	void VulkanVertexBuffer::Bind()
	{
		auto& commandBuffer = Renderer::GetVulkanPresentation()->GetCommandBuffer();

		VkBuffer vertexBuffers[] = { m_VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	}
}
