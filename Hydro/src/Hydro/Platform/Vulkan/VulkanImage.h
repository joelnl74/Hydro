#pragma once
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"
#include "Hydro/Renderer/Image.h"

namespace Hydro
{
	struct VulkanImageInfo
	{
		VkImage Image;
		VkImageView ImageView;
		VkSampler Sampler;
		VmaAllocation MemoryAlloc = nullptr;
		uint32_t size;
	};

	class VulkanImage
	{
	public:
		VulkanImage(const std::string &filePath, ImageSpecification spec);
		const VulkanImageInfo &GetVulkanImageInfo() { return m_vulkanImageInfo; };

		VkBuffer& GetBuffer() { return m_ImageData; };

		void Bind();
	private:
		VkFormat GetImageFormat(ImageFormat imageformat);
	private:
		int m_width, m_heigth, m_texChannels;
		VulkanImageInfo m_vulkanImageInfo;
		VkBuffer m_ImageData;
	};
}

