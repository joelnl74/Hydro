#pragma once
#include <vk_mem_alloc.h>
#include <string>
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"

namespace Hydro
{
	struct GPUMemoryStats
	{
		uint64_t Used = 0;
		uint64_t Free = 0;
	};

	class VulkanAllocator
	{
	public:
		VulkanAllocator() = default;
		VulkanAllocator(const std::string& tag);
		~VulkanAllocator();

		VmaAllocation AllocateBuffer(VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer);
		// With VMA allocation flags (e.g. VMA_ALLOCATION_CREATE_MAPPED_BIT); `outInfo` (optional) receives
		// the allocation's info, its persistent mapping included.
		VmaAllocation AllocateBuffer(VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer, VmaAllocationCreateFlags flags, VmaAllocationInfo* outInfo);
		VmaAllocation AllocateImage(VkImageCreateInfo imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage);
		void Free(VmaAllocation allocation);
		void DestroyImage(VkImage image, VmaAllocation allocation);
		void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

		template<typename T>
		T* MapMemory(VmaAllocation allocation)
		{
			T* mappedMemory;
			vmaMapMemory(VulkanAllocator::GetVMAAllocator(), allocation, (void**)&mappedMemory);
			return mappedMemory;
		}

		void UnmapMemory(VmaAllocation allocation);

		static void DumpStats();
		static GPUMemoryStats GetStats();

		static void Init(VkInstance instance, VulkanDevice& device);
		static void Shutdown();

		static VmaAllocator& GetVMAAllocator();
	private:
		std::string m_Tag;
	};
}
