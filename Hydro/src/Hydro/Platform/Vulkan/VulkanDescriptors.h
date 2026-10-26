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

	// Descriptor set layouts and descriptor sets for the whole renderer (plan M2b). Started and stopped by
	// Renderer::Init / Shutdown.
	//
	// - Layouts come from a cache: one per distinct list of bindings, kept until Shutdown. Builders with the
	//   same bindings share a layout, so their sets and the pipelines built from either are compatible.
	// - Persistent sets (a view's texture sets, materials from M4) come from growable pools and are freed
	//   one by one through the deletion queue, after the frames that may still bind them.
	// - Per-frame sets are valid for the frame being recorded only: their pools belong to that frame in
	//   flight and are reset (all at once) right after its fence has signalled. Never keep one past the frame.
	class VulkanDescriptorAllocator
	{
	public:
		static void Init();
		// After the deletion queue is flushed (persistent sets are freed through it).
		static void Shutdown();

		// The layout for `bindings` (no immutable samplers), made the first time it is asked for.
		static VkDescriptorSetLayout GetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

		static DescriptorAllocation AllocatePersistent(VkDescriptorSetLayout layout);
		// Through the deletion queue: frames in flight may still have it bound.
		static void FreePersistent(const DescriptorAllocation& allocation);

		// A set for the frame being recorded (Renderer::GetRenderFrame()); gone when that frame slot is next used.
		static VkDescriptorSet AllocateFrame(VkDescriptorSetLayout layout);
		// Called by Renderer::BeginFrame after the frame's fence: the frame's pools are reset.
		static void ResetFrame(uint32_t frameIndex);
	};
}
