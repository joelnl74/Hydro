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

	void VulkanShader::Destory()
	{
		auto context = Renderer::GetRendererContext();
		VkDevice device = context->GetVulkanDevice()->GetDevice();

		// TODO clean up all created buffers, descriptor pools and descriptor layouts.
		// vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}

	void VulkanShader::CreateDescriptorSetLayout()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		m_UBOLayoutBinding.binding = 0;
		m_UBOLayoutBinding.descriptorCount = 1;
		m_UBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		m_UBOLayoutBinding.pImmutableSamplers = nullptr;
		m_UBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &m_UBOLayoutBinding;

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	void VulkanShader::CreateDescriptorPool()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void VulkanShader::CreateDescriptorSet(std::vector<VkBuffer> &buffers, uint32_t size)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();

		m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = buffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = (uint32_t)size;

			std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

			for (uint32_t j = 0; j < descriptorWrites.size(); j++)
			{
				descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[j].dstSet = m_DescriptorSets[i];
				descriptorWrites[j].dstBinding = j;
				descriptorWrites[j].dstArrayElement = 0;
				descriptorWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[j].descriptorCount = 1;
				descriptorWrites[j].pBufferInfo = &bufferInfo;
			}

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
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
