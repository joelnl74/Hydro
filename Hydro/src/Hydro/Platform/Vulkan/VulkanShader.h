#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"
#include <map>
namespace Hydro
{
	enum class ShaderType
	{
		None = 0,
		Vertex = 1,
		Fragment = 2
	};

	struct ShaderInformation
	{
		std::string path;
		VkShaderStageFlagBits shaderStage;
		ShaderType shaderType;
	};

	struct ShaderSpecification
	{
		std::vector<ShaderInformation> shaderInformation;
	};

	class VulkanShader
	{
	public:
		VulkanShader(ShaderSpecification spec);
		void Create(const ShaderInformation &info);
		
		// Replace with destructor;
		void Destory();

		VkShaderModule &GetModule(ShaderType type) 
		{
			// TODO ASSERT.
			return m_ShaderModules[(uint32_t)type];
		}

		// Get methods
		const std::vector<VkDescriptorSet>& GetDescriptorSets() { return m_DescriptorSets; };
		const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInformation() { return m_ShaderStages; };
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
		VkDescriptorSetLayoutBinding m_UBOLayoutBinding;

		VkDescriptorPool m_DescriptorPool;

		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
		std::map<uint32_t, VkShaderModule> m_ShaderModules;
	};
}


