#include "hypch.h"
#include "VulkanUniformBuffer.h"

#include "Hydro/Renderer/Renderer.h"
namespace Hydro
{

	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		m_vmaAllocation.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.pNext = nullptr;
			allocInfo.allocationSize = 0;
			allocInfo.memoryTypeIndex = 0;

			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.size = size;

			VulkanAllocator allocator("UniformBuffer");
			m_vmaAllocation[i] = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_ONLY, m_uniformBuffers[i]);
		}
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		if (device == nullptr && m_uniformBuffers.size() <= 0)
		{
			return;
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
			// VulkanAllocator::Free(m_vmaAllocation[i]);
		}
	}

	void VulkanUniformBuffer::Update(const void* data, uint32_t currentImage, uint32_t size)
	{
		VulkanAllocator allocator("VulkanUniformBuffer");
		uint8_t* pData = allocator.MapMemory<uint8_t>(m_vmaAllocation[currentImage]);
		memcpy(pData, (uint8_t*)data, size);
		allocator.UnmapMemory(m_vmaAllocation[currentImage]);
	}

	void VulkanUniformBuffer::Destory()
	{
		VulkanAllocator allocator("VulkanUniformBuffer");

		for (uint32_t i = 0; i < m_uniformBuffers.size(); i++)
		{
			allocator.DestroyBuffer(m_uniformBuffers[i], m_vmaAllocation[i]);
		}
	}

}

