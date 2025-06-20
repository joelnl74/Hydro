#include "hypch.h"

#include "VulkanImage.h"
#include "VulkanAllocator.h"
#include "Hydro/Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanImage::VulkanImage(const std::string& filePath, ImageSpecification spec)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		stbi_uc* imagePixels = stbi_load(filePath.c_str(), &m_width, &m_heigth, &m_texChannels, STBI_rgb_alpha);

		m_vulkanImageInfo.size = m_width * m_heigth * 4;
		VkDeviceSize imageSize = m_vulkanImageInfo.size;

		// TODO assert.
		if (imagePixels == false)
		{
			throw std::runtime_error("failed to load texture image!");
		}

		VulkanAllocator allocator("VulkanImageBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = imageSize;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		auto bufferAlloc = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_ImageData);

		void* dstBuffer = allocator.MapMemory<void>(bufferAlloc);
		memcpy(dstBuffer, imagePixels, imageSize);
		allocator.UnmapMemory(bufferAlloc);

		stbi_image_free(imagePixels);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_width;
		imageInfo.extent.height = m_heigth;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = GetImageFormat(spec.Format);
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_vulkanImageInfo.MemoryAlloc = allocator.AllocateImage(imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_vulkanImageInfo.Image);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = GetImageFormat(spec.Format);
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_vulkanImageInfo.Image;
		
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_vulkanImageInfo.ImageView));

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_vulkanImageInfo.Sampler));
	}

	VulkanImage::VulkanImage(ImageSpecification spec)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		m_vulkanImageInfo.size = spec.Width * spec.Height * 4;
		VkDeviceSize imageSize = m_vulkanImageInfo.size;

		VulkanAllocator allocator("VulkanImageBuffer");

		VkBufferCreateInfo vertexBufferCreateInfo = {};
		vertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferCreateInfo.size = imageSize;
		vertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		VkImageUsageFlags usage = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = spec.Width;
		imageInfo.extent.height = spec.Height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		m_vulkanImageInfo.MemoryAlloc = allocator.AllocateImage(imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_vulkanImageInfo.Image);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_vulkanImageInfo.Image;

		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_vulkanImageInfo.ImageView));

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
		samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_vulkanImageInfo.Sampler));
	}

	void VulkanImage::Bind()
	{

	}

	VkFormat VulkanImage::GetImageFormat(ImageFormat imageformat)
	{
		switch (imageformat)
		{
			case Hydro::ImageFormat::RGBA:		return VK_FORMAT_R8G8B8A8_UNORM;
			case Hydro::ImageFormat::RGBA32F:	return VK_FORMAT_R32G32B32A32_SFLOAT;
			case Hydro::ImageFormat::DEPTH32F:	return VK_FORMAT_D32_SFLOAT;
			default: return VK_FORMAT_R8G8B8A8_UNORM;
		}
	}
}
