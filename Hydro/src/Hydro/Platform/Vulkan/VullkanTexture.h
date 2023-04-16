#pragma once
#include "VulkanCommandBuffer.h"
namespace Hydro
{
	struct VulkanTextureSpecification
	{

	};

	class VullkanTexture
	{
	public:
		VullkanTexture(const std::string &filePath, VulkanTextureSpecification specfication);

		VkSampler& GetImageSampler() { return m_sampler; };
		VkImageView& GetImageView() { return m_textureImageView;  }

	private:
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	/// <summary>
	/// Temp code replace with Vulkan allocator
	/// </summary>
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	private:
		VkBuffer m_buffer;
		VkImage m_image;
		VkImageView m_textureImageView;
		VkSampler m_sampler;

		VkDeviceMemory m_textureImageMemory;
	};
}


