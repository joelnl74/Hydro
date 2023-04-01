#pragma once

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
	private:
		int m_width, m_heigth, m_texChannels;
		VkImage m_image;
		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;
		VmaAllocation m_MemoryAllocation;
	};
}

