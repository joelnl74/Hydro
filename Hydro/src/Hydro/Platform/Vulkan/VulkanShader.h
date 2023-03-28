#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"

namespace Hydro
{
	class VulkanShader
	{
	public:
		VkShaderModule Create(const std::string& filePath);
		void Destory();

		VkShaderModule & GetModule() { return m_ShaderModule; }

		// Get methods
		const std::vector<VkDescriptorSet>& GetDescriptorSets() { return m_DescriptorSets; };
		const VkDescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; };

		// Create methods
		void CreateDescriptorSetLayout();
		void CreateDescriptorPool();
		void CreateDescriptorSet(std::vector<VkBuffer>& buffers, uint32_t size);

		VkDescriptorSetLayoutBinding const & GetDescriptorSetLayoutBinding() { return m_UBOLayoutBinding;  }

	private:
		std::vector<char> readFile(const std::string& filePath);

	private:
		// TODO Make it a ref.
		VkDescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorPool m_DescriptorPool;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		VkShaderModule m_ShaderModule;
		VkDescriptorSetLayoutBinding m_UBOLayoutBinding;
	};
}


