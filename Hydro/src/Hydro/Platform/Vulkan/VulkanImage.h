#pragma once
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"

namespace Hydro
{
	struct VulkanImageSpecification
	{
		VkFormat format; 
		VkImageTiling tiling; 
		VkImageUsageFlags usage; 
		VkMemoryPropertyFlags properties;
	};

	class VulkanImage
	{
	public:
		VulkanImage(const std::string &filePath, VulkanImageSpecification spec);
		void Bind();
	private:
		int m_width, m_heigth, m_texChannels;

		VkImage m_image;
		VkImageView m_imageView;
		VkSampler m_sampler;

		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;
		VmaAllocation m_MemoryAllocation;
	};
}

