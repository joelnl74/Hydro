#include "hypch.h"
#include "VulkanDescriptorBuilder.h"
#include "VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	void VulkanDescriptorBuilder::Begin()
	{
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_writeDescriptorSets.emplace(i, std::vector<PendingWrite>());
		}
	}

	void VulkanDescriptorBuilder::BindBuffer(uint32_t binding, Ref<VulkanUniformBuffer> &buffer, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		const uint32_t descriptorCount = 1;

		// One descriptor per set, one set per frame in flight.
		VkDescriptorPoolSize poolSize{};
		poolSize.type = type;
		poolSize.descriptorCount = descriptorCount * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		m_poolSizes.push_back(poolSize);

		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = binding;
		uboLayoutBinding.descriptorCount = descriptorCount;
		uboLayoutBinding.descriptorType = type;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = stageFlags;

		m_layoutBinding.push_back(uboLayoutBinding);

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = buffer->GetVKBuffers()[i];
			bufferInfo.offset = 0;
			bufferInfo.range = (VkDeviceSize)size;

			PendingWrite pending;
			pending.InfoOffset = m_bufferInfo.size();
			pending.IsImage = false;
			m_bufferInfo.push_back(bufferInfo);

			pending.Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			pending.Write.dstBinding = binding;
			pending.Write.dstArrayElement = 0;
			pending.Write.descriptorType = type;
			pending.Write.descriptorCount = descriptorCount;
			m_writeDescriptorSets[i].push_back(pending);
		}
	}

	void VulkanDescriptorBuilder::BindImage(uint32_t binding, const Ref<VullkanTexture>& vulkanTexture, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		BindImage(binding, vulkanTexture->GetImageView(), vulkanTexture->GetImageSampler(), type, stageFlags);
	}

	void VulkanDescriptorBuilder::BindImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		const uint32_t descriptorCount = MaxImageArraySize;

		// The binding is an array, so the pool needs the whole array for every frame's set.
		VkDescriptorPoolSize poolSize{};
		poolSize.type = type;
		poolSize.descriptorCount = descriptorCount * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		m_poolSizes.push_back(poolSize);

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = binding;
		samplerLayoutBinding.descriptorCount = descriptorCount;
		samplerLayoutBinding.descriptorType = type;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = stageFlags;

		m_layoutBinding.push_back(samplerLayoutBinding);

		// Fill every slot, so the shader never reads an unwritten descriptor whatever index it uses.
		const size_t infoOffset = m_imageInfo.size();
		for (uint32_t i = 0; i < descriptorCount; i++)
		{
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = imageView;
			imageInfo.sampler = sampler;
			m_imageInfo.push_back(imageInfo);
		}

		// The image infos don't change per frame, so every frame's write points at the same array.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			PendingWrite pending;
			pending.InfoOffset = infoOffset;
			pending.IsImage = true;

			pending.Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			pending.Write.dstBinding = binding;
			pending.Write.dstArrayElement = 0;
			pending.Write.descriptorType = type;
			pending.Write.descriptorCount = descriptorCount;
			m_writeDescriptorSets[i].push_back(pending);
		}
	}

	bool VulkanDescriptorBuilder::Build()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(m_layoutBinding.size());
		layoutInfo.pBindings = m_layoutBinding.data();

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout));

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(m_poolSizes.size());
		poolInfo.pPoolSizes = m_poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool));

		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();

		m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()));

		// All Bind* calls are done, so the info vectors no longer move: resolve the pointers now.
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			std::vector<VkWriteDescriptorSet> writes;
			writes.reserve(m_writeDescriptorSets[i].size());

			for (auto& pending : m_writeDescriptorSets[i])
			{
				VkWriteDescriptorSet write = pending.Write;
				write.dstSet = m_DescriptorSets[i];

				if (pending.IsImage)
				{
					write.pImageInfo = &m_imageInfo[pending.InfoOffset];
				}
				else
				{
					write.pBufferInfo = &m_bufferInfo[pending.InfoOffset];
				}

				writes.push_back(write);
			}

			vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
		}

		return true;
	}

	void VulkanDescriptorBuilder::WriteImages(uint32_t frameIndex, uint32_t binding, const VkDescriptorImageInfo* images, uint32_t count, VkDescriptorType type)
	{
		HY_CORE_ASSERT(frameIndex < m_DescriptorSets.size(), "VulkanDescriptorBuilder::WriteImages before Build");
		HY_CORE_ASSERT(count <= MaxImageArraySize, "VulkanDescriptorBuilder::WriteImages: too many images");
		if (count == 0)
		{
			return;
		}

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_DescriptorSets[frameIndex];
		write.dstBinding = binding;
		write.dstArrayElement = 0;
		write.descriptorType = type;
		write.descriptorCount = count;
		write.pImageInfo = images;

		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void VulkanDescriptorBuilder::Destroy()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		m_bufferInfo.clear();
		m_imageInfo.clear();
		m_writeDescriptorSets.clear();

		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
		m_DescriptorPool = VK_NULL_HANDLE;
		m_DescriptorSetLayout = VK_NULL_HANDLE;
	}
}
