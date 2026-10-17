#pragma once
#include <vulkan/vulkan.h>

#include "VullkanTexture.h"
#include "VulkanUniformBuffer.h"

#include <vector>
#include <map>

namespace Hydro
{
	class VulkanDescriptorBuilder
	{
	public:
		// Size of an image array binding (must match the sampler array in the shader).
		static constexpr uint32_t MaxImageArraySize = 32;

		// Creation methods.
		void Begin();
		void BindBuffer(uint32_t binding, Ref<VulkanUniformBuffer>& buffer, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags);
		// Binds an array of MaxImageArraySize images. Every slot is written with `vulkanTexture`,
		// so a shader that indexes any slot always reads a valid descriptor.
		void BindImage(uint32_t binding, Ref<VullkanTexture> &vulkanTexture, VkDescriptorType type, VkShaderStageFlags stageFlags);
		bool Build();
		void Destroy();

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
