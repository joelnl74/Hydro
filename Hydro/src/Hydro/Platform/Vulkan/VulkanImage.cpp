#include "hypch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <vendor/stb/include/stb_image.h>
#include "vendor/VulkanMemoryAllocator/vk_mem_alloc.h"

#include "VulkanImage.h"
#include "VulkanAllocator.h"
#include "Hydro/Renderer/Renderer.h"

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
	}
}
