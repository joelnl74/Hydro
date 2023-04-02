#include "hypch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <vendor/stb/include/stb_image.h>

#include "VulkanImage.h"
#include "VulkanAllocator.h"
#include "Hydro/Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanImage::VulkanImage(const std::string& filePath, VulkanImageSpecification spec)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		stbi_uc* imagePixels = stbi_load("textures/texture.jpg", &m_width, &m_heigth, &m_texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = m_width * m_heigth * 4;

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

		auto bufferAlloc = allocator.AllocateBuffer(vertexBufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_VertexBuffer);

		void* dstBuffer = allocator.MapMemory<void>(bufferAlloc);
		memcpy(dstBuffer, imagePixels, imageSize);
		allocator.UnmapMemory(bufferAlloc);

		stbi_image_free(imagePixels);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_width;
		imageInfo.extent.height = m_heigth;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = spec.format;
		imageInfo.tiling = spec.tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = spec.usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		allocator.AllocateImage(imageInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, m_image);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = spec.format;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.subresourceRange = {};
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_image;
		
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_imageView));

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

		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_sampler));
	}

	void VulkanImage::Bind()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
	}
}
