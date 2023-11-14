#include "hypch.h"
#include "VulkanDescriptorBuilder.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	void VulkanDescriptorBuilder::Begin()
	{
		m_writeDescriptorSets.emplace(0, std::vector<VkWriteDescriptorSet>());
		m_writeDescriptorSets.emplace(1, std::vector<VkWriteDescriptorSet>());
	}

	void VulkanDescriptorBuilder::BindBuffer(uint32_t binding, Ref<VulkanUniformBuffer> &buffer, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorPoolSize poolSize;
		poolSize.type = type;
		poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		m_poolSizes.push_back(poolSize);

		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = binding;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = type;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = stageFlags;

		m_layoutBinding.push_back(uboLayoutBinding);

		for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkDescriptorBufferInfo *bufferInfo = new VkDescriptorBufferInfo();
			bufferInfo->buffer = buffer->GetVKBuffers()[i];
			bufferInfo->offset = 0;
			bufferInfo->range = (uint32_t)size;
			m_bufferInfo.push_back(bufferInfo);

			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			m_writeDescriptorSets[i].push_back(writeDescriptorSet);
		}
	}

	void VulkanDescriptorBuilder::BindImage(uint32_t binding, Ref<VullkanTexture>& vulkanTexture, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorPoolSize poolSize;
		poolSize.type = type;
		poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		m_poolSizes.push_back(poolSize);

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = binding;
		samplerLayoutBinding.descriptorCount = 32;
		samplerLayoutBinding.descriptorType = type;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = stageFlags;

		m_layoutBinding.push_back(samplerLayoutBinding);


		VkDescriptorImageInfo* imageInfo[32];
		
		for (unsigned int i = 0; i < 32; i++)
		{
			imageInfo[i] = new VkDescriptorImageInfo();

			imageInfo[i]->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo[i]->imageView = vulkanTexture->GetImageView();
			imageInfo[i]->sampler = vulkanTexture->GetImageSampler();
		}

		for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkWriteDescriptorSet writeDescriptorSet{};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.pImageInfo = imageInfo[0];
			m_writeDescriptorSets[i].push_back(writeDescriptorSet);
		}
	}

	bool VulkanDescriptorBuilder::Build()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(m_layoutBinding.size());
		layoutInfo.pBindings = m_layoutBinding.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(m_poolSizes.size());
		poolInfo.pPoolSizes = m_poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();

		m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			for (auto &x : m_writeDescriptorSets[i])
			{
				x.dstSet = m_DescriptorSets[i];
				x.dstSet = m_DescriptorSets[i];
			}

			vkUpdateDescriptorSets(device, (uint32_t)m_writeDescriptorSets[i].size(), m_writeDescriptorSets[i].data(), 0, nullptr);
		}

		return true;
	}

	void VulkanDescriptorBuilder::Destroy()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		for (auto info : m_bufferInfo)
		{
			delete info;
		}

		for (auto info : m_imageInfo)
		{
			delete info;
		}

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	}
}
