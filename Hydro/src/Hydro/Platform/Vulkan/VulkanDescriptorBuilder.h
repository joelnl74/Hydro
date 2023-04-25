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

		// Creation methods.
		void Begin();
		void BindBuffer(uint32_t binding, Ref<VulkanUniformBuffer>& buffer, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags);
		void BindImage(uint32_t binding, Ref<VullkanTexture> &vulkanTexture, VkDescriptorType type, VkShaderStageFlags stageFlags);
		bool Build();

		// Get methods.
		VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; };
		VkDescriptorSet& GetDescriptorSet(uint32_t index) { return m_DescriptorSets[index]; };
	private:
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::map<uint32_t, std::vector<VkWriteDescriptorSet>> m_writeDescriptorSets;
		std::vector<VkDescriptorSetLayoutBinding> m_layoutBinding;
		std::vector<VkDescriptorPoolSize> m_poolSizes;
		std::vector<VkDescriptorBufferInfo*> m_info;
		VkDescriptorPool m_DescriptorPool;
		VkDescriptorSetLayout m_DescriptorSetLayout;
	};
}
