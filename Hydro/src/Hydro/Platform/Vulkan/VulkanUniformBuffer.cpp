#include "hypch.h"
#include "VulkanUniformBuffer.h"

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

namespace Hydro
{
	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size)
		: m_Size(size)
	{
		m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		m_VmaAllocations.resize(MAX_FRAMES_IN_FLIGHT);

		VulkanAllocator allocator("UniformBuffer");
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.size = size;

			m_VmaAllocations[i] = allocator.AllocateBuffer(bufferInfo, VMA_MEMORY_USAGE_CPU_ONLY, m_UniformBuffers[i]);
		}
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		if (m_UniformBuffers.empty())
		{
			return;
		}

		Renderer::SubmitResourceFree([buffers = m_UniformBuffers, allocations = m_VmaAllocations]()
		{
			VulkanAllocator allocator("UniformBuffer");
			for (size_t i = 0; i < buffers.size(); i++)
			{
				allocator.DestroyBuffer(buffers[i], allocations[i]);
			}
		});
	}

	void VulkanUniformBuffer::Update(const void* data, uint32_t frameIndex, uint32_t size)
	{
		HY_CORE_ASSERT(size <= m_Size, "VulkanUniformBuffer::Update: more data than the buffer holds");

		VulkanAllocator allocator("UniformBuffer");
		uint8_t* mapped = allocator.MapMemory<uint8_t>(m_VmaAllocations[frameIndex]);
		memcpy(mapped, data, size);
		allocator.UnmapMemory(m_VmaAllocations[frameIndex]);
	}
}
