#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"
#include "VullkanTexture.h"
#include "VulkanDescriptorBuilder.h"

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
		VulkanDescriptorBuilder descriptorBuilder;
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
		const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInformation() { return m_ShaderStages; };
		
	private:
		std::vector<char> readFile(const std::string& filePath);

	private:
		// TODO Make it a ref.
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
		std::map<uint32_t, VkShaderModule> m_ShaderModules;
	};
}


