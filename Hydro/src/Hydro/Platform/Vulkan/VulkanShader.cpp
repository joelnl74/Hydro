#include "hypch.h"
#include "VulkanShader.h"

#include "VulkanRendererContext.h";
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VkShaderModule VulkanShader::Create(const std::string& filePath)
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
		
		return m_ShaderModule;
	}

	void VulkanShader::Destory()
	{
		auto context = Renderer::GetRendererContext();
		VkDevice device = context->GetVulkanDevice()->GetDevice();

		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}

	void VulkanShader::CreateDescriptorSetLayout()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		VK_SUCCESS(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout));
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
