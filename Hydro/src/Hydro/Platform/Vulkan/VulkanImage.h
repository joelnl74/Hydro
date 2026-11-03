#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Renderer/Image.h"
#include <vk_mem_alloc.h>

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

	// Owns a GPU image, its view and a sampler, freed through Renderer::SubmitResourceFree.
	class VulkanImage
	{
	public:
		// RGBA8 from disk, left in SHADER_READ_ONLY_OPTIMAL.
		VulkanImage(const std::string& filePath, ImageSpecification spec, const TextureProperties& properties = {});
		// From width * height RGBA8 pixels (Format RGBA or SRGB), left in SHADER_READ_ONLY_OPTIMAL.
		VulkanImage(const ImageSpecification& spec, const void* pixels, const TextureProperties& properties = {});
		// Usage::Attachment makes a render target that can also be sampled.
		explicit VulkanImage(ImageSpecification spec);
		~VulkanImage();

		VulkanImage(const VulkanImage&) = delete;
		VulkanImage& operator=(const VulkanImage&) = delete;

		// Replaces a rectangle of RGBA8 pixels (rows top to bottom, tightly packed). Waits for the GPU: editor edits only.
		void UpdatePixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* pixels);

		const VulkanImageInfo& GetVulkanImageInfo() const { return m_VulkanImageInfo; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		VkFormat GetFormat() const { return m_Format; }

		static VkFormat GetImageFormat(ImageFormat imageformat);
		static bool IsDepthFormat(ImageFormat imageformat);
		static VkFilter GetFilter(TextureFilter filter);
		static VkSamplerAddressMode GetAddressMode(TextureWrap wrap);

	private:
		void CreateImage(VkImageUsageFlags usage);
		void CreateViewAndSampler(VkImageAspectFlags aspect, const TextureProperties& properties);
		void UploadPixels(const void* pixels, VkDeviceSize size);

	private:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		VkFormat m_Format = VK_FORMAT_UNDEFINED;
		VulkanImageInfo m_VulkanImageInfo;
	};
}
