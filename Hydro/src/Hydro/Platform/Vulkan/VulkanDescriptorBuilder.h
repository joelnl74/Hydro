#pragma once
#include <vulkan/vulkan.h>

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"

#include <vector>
#include <map>

namespace Hydro
{
	// One persistent descriptor set per frame in flight, described with Begin, Bind... and Build. The layout comes
	// from VulkanDescriptorAllocator's cache, so builders with the same bindings are compatible.
	class VulkanDescriptorBuilder
	{
	public:
		// Must match the sampler array in the shader.
		static constexpr uint32_t MaxImageArraySize = 32;

		VulkanDescriptorBuilder() = default;
		~VulkanDescriptorBuilder();

		VulkanDescriptorBuilder(const VulkanDescriptorBuilder&) = delete;
		VulkanDescriptorBuilder& operator=(const VulkanDescriptorBuilder&) = delete;

		void Begin();
		// Each frame's set points at that frame's buffer, which must outlive the sets' use.
		void BindBuffer(uint32_t binding, const PerFrameBuffers& buffers, uint32_t size, VkDescriptorType type,
			VkShaderStageFlags stageFlags);
		// An array of `count` images (1 for a single sampler), every slot written with the given image so any index is
		// valid.
		void BindImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags stageFlags,
			uint32_t count = MaxImageArraySize);
		bool Build();

		// After Build: rewrites elements 0..count-1 of the image array at `binding` in frame `frameIndex`'s set. Only
		// after that frame's fence and before the set is bound this frame.
		void WriteImages(uint32_t frameIndex, uint32_t binding, const VkDescriptorImageInfo* images, uint32_t count,
			VkDescriptorType type);

		VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; };
		VkDescriptorSet& GetDescriptorSet(uint32_t index) { return m_DescriptorSets[index]; };
	private:
		// pBufferInfo / pImageInfo are resolved in Build, once the info vectors stop growing.
		struct PendingWrite
		{
			VkWriteDescriptorSet Write{};
			size_t InfoOffset = 0;
			bool IsImage = false;
		};

		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<DescriptorAllocation> m_Allocations;
		std::map<uint32_t, std::vector<PendingWrite>> m_WriteDescriptorSets;
		std::vector<VkDescriptorSetLayoutBinding> m_LayoutBinding;

		std::vector<VkDescriptorBufferInfo> m_BufferInfo;
		std::vector<VkDescriptorImageInfo> m_ImageInfo;

		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE; // owned by the layout cache
	};
}
