#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Hydro
{
	// A long-lived descriptor set and the pool it came from (freeing it needs the pool).
	struct DescriptorAllocation
	{
		VkDescriptorSet Set = VK_NULL_HANDLE;
		VkDescriptorPool Pool = VK_NULL_HANDLE;
	};

	// Descriptor set layouts (cached per binding list, so equal bindings are compatible), persistent sets (freed
	// through the deletion queue) and per-frame sets (reset after the frame's fence: never keep one past the frame).
	class VulkanDescriptorAllocator
	{
	public:
		static void Init();
		// After the deletion queue is flushed (persistent sets are freed through it).
		static void Shutdown();

		// Made the first time it is asked for (no immutable samplers).
		static VkDescriptorSetLayout GetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

		static DescriptorAllocation AllocatePersistent(VkDescriptorSetLayout layout);
		// Through the deletion queue: frames in flight may still have it bound.
		static void FreePersistent(const DescriptorAllocation& allocation);

		// Valid for the frame being recorded only.
		static VkDescriptorSet AllocateFrame(VkDescriptorSetLayout layout);
		// After the frame's fence (Renderer::BeginFrame).
		static void ResetFrame(uint32_t frameIndex);
	};
}
