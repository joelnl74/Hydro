#include "hypch.h"
#include "VulkanBuffer.h"

#include "Hydro/Platform/Vulkan/VulkanAllocator.h"
#include "Hydro/Renderer/Renderer.h"

#include <cstring>

namespace Hydro
{
	static VkBufferUsageFlags GetVulkanUsage(BufferUsage usage)
	{
		switch (usage)
		{
			case BufferUsage::Vertex:  return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case BufferUsage::Index:   return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			case BufferUsage::Uniform: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case BufferUsage::Storage: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			case BufferUsage::Staging: return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		}
		return 0;
	}

	VulkanBuffer::VulkanBuffer(const BufferSpecification& specification, const void* data)
		: m_Specification(specification)
	{
		HY_CORE_ASSERT(specification.Size > 0, "VulkanBuffer: Size must not be 0");
		const bool gpuOnly = specification.Memory == BufferMemory::GpuOnly;
		HY_CORE_ASSERT(gpuOnly == false || specification.Usage != BufferUsage::Staging, "VulkanBuffer: a staging buffer must be HostVisible");
		HY_CORE_ASSERT(gpuOnly == false || data != nullptr, "VulkanBuffer: a GpuOnly buffer needs its data at creation");

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = (VkDeviceSize)specification.Size;
		bufferInfo.usage = GetVulkanUsage(specification.Usage) | (gpuOnly ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		// Staging memory only needs to be visible to the CPU; the rest of the host-visible buffers are read by
		// the GPU every frame, so VMA prefers memory the GPU reads quickly. Host-visible buffers are mapped
		// once, for their whole life (VMA unmaps them when they are destroyed).
		VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
		VmaAllocationCreateFlags flags = 0;
		if (gpuOnly == false)
		{
			memoryUsage = specification.Usage == BufferUsage::Staging ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_CPU_TO_GPU;
			flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		VulkanAllocator allocator(specification.DebugName);
		VmaAllocationInfo allocationInfo{};
		m_Allocation = allocator.AllocateBuffer(bufferInfo, memoryUsage, m_Buffer, flags, &allocationInfo);
		m_Mapped = allocationInfo.pMappedData;

		if (data == nullptr)
		{
			return;
		}

		if (gpuOnly == false)
		{
			SetData(data, specification.Size);
			return;
		}

		// GpuOnly: through a staging buffer. SubmitImmediate waits, so the staging buffer can go right after
		// (it is freed through the deletion queue anyway).
		BufferSpecification stagingSpecification;
		stagingSpecification.Usage = BufferUsage::Staging;
		stagingSpecification.Memory = BufferMemory::HostVisible;
		stagingSpecification.Size = specification.Size;
		stagingSpecification.DebugName = specification.DebugName + " (staging)";
		VulkanBuffer staging(stagingSpecification, data);

		const VkBuffer source = staging.GetVulkanBuffer();
		const VkBuffer destination = m_Buffer;
		const VkDeviceSize size = (VkDeviceSize)specification.Size;
		Renderer::GetVulkanDevice().SubmitImmediate([source, destination, size](VkCommandBuffer commandBuffer)
		{
			VkBufferCopy region{};
			region.size = size;
			vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region);

			// Make the copy visible to every later read of the buffer (vertex input, index, uniform, shader).
			VkMemoryBarrier2 barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

			VkDependencyInfo dependency{};
			dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dependency.memoryBarrierCount = 1;
			dependency.pMemoryBarriers = &barrier;
			vkCmdPipelineBarrier2(commandBuffer, &dependency);
		});
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (m_Buffer == VK_NULL_HANDLE)
		{
			return;
		}

		// A frame in flight may still read it: free it once those frames are done.
		Renderer::SubmitResourceFree([buffer = m_Buffer, allocation = m_Allocation, name = m_Specification.DebugName]()
		{
			VulkanAllocator allocator(name);
			allocator.DestroyBuffer(buffer, allocation);
		});
	}

	void VulkanBuffer::SetData(const void* data, size_t size, size_t offset)
	{
		HY_CORE_ASSERT(m_Mapped != nullptr, "VulkanBuffer::SetData: only HostVisible buffers can be written");
		HY_CORE_ASSERT(offset + size <= m_Specification.Size, "VulkanBuffer::SetData: more data than the buffer holds");
		if (m_Mapped == nullptr || size == 0)
		{
			return;
		}

		std::memcpy((uint8_t*)m_Mapped + offset, data, size);
		Flush(offset, size);
	}

	void VulkanBuffer::Flush(size_t offset, size_t size)
	{
		// Rounds to nonCoherentAtomSize itself; does nothing on HOST_COHERENT memory.
		vmaFlushAllocation(VulkanAllocator::GetVMAAllocator(), m_Allocation, (VkDeviceSize)offset, (VkDeviceSize)size);
	}

	void VulkanBuffer::BindAsVertexBuffer(VkCommandBuffer commandBuffer, uint32_t binding) const
	{
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, binding, 1, &m_Buffer, &offset);
	}

	void VulkanBuffer::BindAsIndexBuffer(VkCommandBuffer commandBuffer) const
	{
		vkCmdBindIndexBuffer(commandBuffer, m_Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	PerFrameBuffers CreatePerFrameBuffers(const BufferSpecification& specification)
	{
		HY_CORE_ASSERT(specification.Memory == BufferMemory::HostVisible, "CreatePerFrameBuffers: per-frame buffers are written by the CPU");
		PerFrameBuffers buffers;
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			BufferSpecification frameSpecification = specification;
			frameSpecification.DebugName = specification.DebugName + " [" + std::to_string(i) + "]";
			buffers[i] = CreateScope<VulkanBuffer>(frameSpecification);
		}
		return buffers;
	}
}
