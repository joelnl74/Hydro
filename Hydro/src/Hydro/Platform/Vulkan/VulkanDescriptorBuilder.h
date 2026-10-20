#pragma once
#include <vulkan/vulkan.h>

#include "VulkanUniformBuffer.h"

#include <vector>
#include <map>

namespace Hydro
{
	// A descriptor set layout, a pool and one descriptor set per frame in flight, described with
	// Begin -> Bind... -> Build. A resource: owns the layout and pool (freed through
	// Renderer::SubmitResourceFree when it is destroyed) and is non-copyable; hold it in a Scope.
	class VulkanDescriptorBuilder
	{
	public:
		// Largest image array binding (the sampler array in the shader must have the same size as the binding).
		static constexpr uint32_t MaxImageArraySize = 32;

		VulkanDescriptorBuilder() = default;
		~VulkanDescriptorBuilder();

		VulkanDescriptorBuilder(const VulkanDescriptorBuilder&) = delete;
		VulkanDescriptorBuilder& operator=(const VulkanDescriptorBuilder&) = delete;

		// Creation methods.
		void Begin();
		// Each frame's set points at that frame's buffer. `buffer` must outlive the descriptor sets' use.
		void BindBuffer(uint32_t binding, const VulkanUniformBuffer& buffer, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags);
		// Binds an array of `count` (<= MaxImageArraySize) images. Every slot is written with the given
		// image, so a shader that indexes any slot always reads a valid descriptor.
		void BindImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t count = MaxImageArraySize);
		bool Build();

		// After Build: rewrites `count` (<= MaxImageArraySize) elements, starting at element 0, of the
		// image array at `binding` in the set of frame `frameIndex`. Only call it once the GPU is done
		// with that frame's set (after the frame's fence) and before the set is bound this frame.
		void WriteImages(uint32_t frameIndex, uint32_t binding, const VkDescriptorImageInfo* images, uint32_t count, VkDescriptorType type);

		// Get methods.
		VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; };
		VkDescriptorSet& GetDescriptorSet(uint32_t index) { return m_DescriptorSets[index]; };
	private:
		// A write whose pBufferInfo / pImageInfo is resolved in Build(), once the info vectors
		// have stopped growing and their addresses are stable.
		struct PendingWrite
		{
			VkWriteDescriptorSet Write{};
			size_t InfoOffset = 0;
			bool IsImage = false;
		};

		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::map<uint32_t, std::vector<PendingWrite>> m_writeDescriptorSets;
		std::vector<VkDescriptorSetLayoutBinding> m_layoutBinding;
		std::vector<VkDescriptorPoolSize> m_poolSizes;

		std::vector<VkDescriptorBufferInfo> m_bufferInfo;
		std::vector<VkDescriptorImageInfo> m_imageInfo;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
	};
}
