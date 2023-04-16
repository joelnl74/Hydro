#include "hypch.h"
#include "VulkanIndexBuffer.h"

#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanIndexBuffer::VulkanIndexBuffer(void *sourceData, uint32_t size)
	{
		VulkanAllocator allocator("IndexBuffer");

		VkBufferCreateInfo indexbufferCreateInfo = {};
		indexbufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexbufferCreateInfo.size = size;
		indexbufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		auto bufferAlloc = allocator.AllocateBuffer(indexbufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_Buffer);

		void* dstBuffer = allocator.MapMemory<void>(bufferAlloc);
		memcpy(dstBuffer, sourceData, size);
		allocator.UnmapMemory(bufferAlloc);

		auto& device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
	}

	void VulkanIndexBuffer::Bind()
	{
		auto& commandBuffer = Renderer::GetVulkanSwapChain()->GetCommandBuffer();

		vkCmdBindIndexBuffer(commandBuffer, m_Buffer, 0, VK_INDEX_TYPE_UINT16);
	}

}
