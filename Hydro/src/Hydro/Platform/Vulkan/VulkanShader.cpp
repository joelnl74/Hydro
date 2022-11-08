#include "hypch.h"
#include "VulkanShader.h"

#include "VulkanRendererContext.h";
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VkShaderModule VulkanShader::Create(const std::string& filePath, VkShaderStageFlagBits bit)
	{
		auto context = Renderer::GetRendererContext();

		VkDevice device = context->GetVulkanDevice()->GetDevice();

		auto code = readFile(filePath);

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		if (vkCreateShaderModule(device, &createInfo, nullptr, &m_ShaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}

		//////////////////////////////////////////////////////////////////////
		// Descriptor Set Layout
		//////////////////////////////////////////////////////////////////////
		m_UBOLayoutBinding.binding = 0;
		m_UBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		m_UBOLayoutBinding.descriptorCount = 1;
		m_UBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		m_UBOLayoutBinding.pImmutableSamplers = nullptr; // Optional

		m_PipelineShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		m_PipelineShaderInfo.stage = bit;
		m_PipelineShaderInfo.module = m_ShaderModule;
		m_PipelineShaderInfo.pName = "main";
		
		return m_ShaderModule;
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
