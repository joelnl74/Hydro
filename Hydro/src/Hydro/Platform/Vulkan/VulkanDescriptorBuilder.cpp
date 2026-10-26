#include "hypch.h"
#include "VulkanDescriptorBuilder.h"
#include "VulkanUtils.h"
#include "VulkanDescriptors.h"
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

	VulkanDescriptorBuilder::~VulkanDescriptorBuilder()
	{
		// Frames in flight may still have the sets bound: they are freed through the deletion queue. The
		// layout belongs to the cache.
		for (const DescriptorAllocation& allocation : m_Allocations)
		{
			VulkanDescriptorAllocator::FreePersistent(allocation);
		}
	}

	void VulkanDescriptorBuilder::BindBuffer(uint32_t binding, const PerFrameBuffers& buffers, uint32_t size, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		const uint32_t descriptorCount = 1;

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
			bufferInfo.buffer = buffers[i]->GetVulkanBuffer();
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

	void VulkanDescriptorBuilder::BindImage(uint32_t binding, VkImageView imageView, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags stageFlags,
		uint32_t count)
	{
		HY_CORE_ASSERT(count > 0 && count <= MaxImageArraySize, "VulkanDescriptorBuilder::BindImage: count must be 1..MaxImageArraySize");
		const uint32_t descriptorCount = count;

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
		auto device = Renderer::GetVulkanDevice().GetDevice();

		HY_CORE_ASSERT(m_DescriptorSets.empty(), "VulkanDescriptorBuilder::Build called twice");

		m_DescriptorSetLayout = VulkanDescriptorAllocator::GetLayout(m_layoutBinding);
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			const DescriptorAllocation allocation = VulkanDescriptorAllocator::AllocatePersistent(m_DescriptorSetLayout);
			m_Allocations.push_back(allocation);
			m_DescriptorSets.push_back(allocation.Set);
		}

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

		auto device = Renderer::GetVulkanDevice().GetDevice();
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}
}
