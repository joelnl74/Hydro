#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"

namespace Hydro
{
	class VulkanShader
	{
	public:
		VkShaderModule Create(const std::string& filePath, VkShaderStageFlagBits bit);
		VkShaderModule GetModule() { return m_ShaderModule; }

		VkDescriptorSetLayoutBinding const & GetDescriptorSetLayoutBinding() { return m_UBOLayoutBinding;  }
		VkPipelineShaderStageCreateInfo const& GetPipelineShaderStageInfo() { return m_PipelineShaderInfo; }

	private:
		std::vector<char> readFile(const std::string& filePath);

	private:
		// TODO Make it a ref.
		VkShaderModule m_ShaderModule;
		VkPipelineShaderStageCreateInfo m_PipelineShaderInfo;
		VkDescriptorSetLayoutBinding m_UBOLayoutBinding;
	};
}


