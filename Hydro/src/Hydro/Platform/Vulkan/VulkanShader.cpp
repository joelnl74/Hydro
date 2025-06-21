#include "hypch.h"
#include "VulkanShader.h"

#include "VulkanRendererContext.h";
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VulkanShader::VulkanShader(ShaderSpecification spec)
	{
		for (auto& shaderInfo : spec.shaderInformation)
		{
			Create(shaderInfo);
		}
	}

	void VulkanShader::Create(const ShaderInformation& info)
	{
		auto &context = Renderer::GetRendererContext();
		auto &device = context->GetVulkanDevice()->GetDevice();
		auto &code = readFile(info.path);
		VkShaderModule shaderModule = nullptr;

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = (uint32_t)code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = info.shaderStage;
		shaderStageInfo.module = shaderModule;
		shaderStageInfo.pName = "main";

		m_ShaderModules.insert({(uint32_t)info.shaderType, shaderModule });
		m_ShaderStages.push_back(shaderStageInfo);
	}

	void VulkanShader::Destroy()
	{
		auto context = Renderer::GetRendererContext();
		VkDevice device = context->GetVulkanDevice()->GetDevice();

		for (auto &x : m_ShaderModules)
		{
			vkDestroyShaderModule(device, x.second, nullptr);
		}
	}

	std::vector<char> VulkanShader::readFile(const std::string& filePath)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) 
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}
}
