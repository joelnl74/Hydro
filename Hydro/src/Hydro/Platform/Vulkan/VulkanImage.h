#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Renderer/Image.h"
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <string>

namespace Hydro
{
	struct VulkanImageInfo
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView ImageView = VK_NULL_HANDLE;
		VkSampler Sampler = VK_NULL_HANDLE;
		VmaAllocation MemoryAlloc = nullptr;
		uint32_t size = 0;
	};

	// Owns a GPU image, its view and a sampler. Non-copyable: copies would double-destroy the handles.
	class VulkanImage
	{
	public:
		// Texture loaded from disk (RGBA8), uploaded and left in SHADER_READ_ONLY_OPTIMAL.
		VulkanImage(const std::string& filePath, ImageSpecification spec);
		// Empty image. Usage::Attachment makes a render target that can also be sampled.
		VulkanImage(ImageSpecification spec);
		~VulkanImage();

		VulkanImage(const VulkanImage&) = delete;
		VulkanImage& operator=(const VulkanImage&) = delete;

		const VulkanImageInfo& GetVulkanImageInfo() const { return m_vulkanImageInfo; }
		uint32_t GetWidth() const { return m_width; }
		uint32_t GetHeight() const { return m_height; }
		VkFormat GetFormat() const { return m_format; }

		static VkFormat GetImageFormat(ImageFormat imageformat);
		static bool IsDepthFormat(ImageFormat imageformat);

	private:
		void CreateImage(VkImageUsageFlags usage);
		void CreateViewAndSampler(VkImageAspectFlags aspect);
		void UploadPixels(const void* pixels, VkDeviceSize size);

	private:
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		VulkanImageInfo m_vulkanImageInfo;
	};
}
