#include "hypch.h"

#include "VulkanImage.h"
#include "VulkanAllocator.h"
#include "VulkanCommandBuffer.h"
#include "Hydro/Renderer/Renderer.h"
#include "VulkanUtils.h"

#include <stb_image.h>

namespace Hydro
{
	VulkanImage::VulkanImage(const std::string& filePath, ImageSpecification spec, const TextureProperties& properties)
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc* imagePixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (imagePixels == nullptr)
		{
			HY_CORE_ERROR("Failed to load image '{0}': {1}", filePath, stbi_failure_reason());
			throw std::runtime_error("failed to load texture image!");
		}

		m_width = (uint32_t)width;
		m_height = (uint32_t)height;
		m_format = GetImageFormat(spec.Format);
		m_vulkanImageInfo.size = m_width * m_height * 4;

		CreateImage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		UploadPixels(imagePixels, m_vulkanImageInfo.size);
		stbi_image_free(imagePixels);

		CreateViewAndSampler(VK_IMAGE_ASPECT_COLOR_BIT, properties);
	}

	VulkanImage::VulkanImage(const ImageSpecification& spec, const void* pixels, const TextureProperties& properties)
	{
		HY_CORE_ASSERT(spec.Width > 0 && spec.Height > 0, "VulkanImage: a texture needs a size");
		HY_CORE_ASSERT(spec.Format == ImageFormat::RGBA || spec.Format == ImageFormat::SRGB, "VulkanImage: pixel uploads are RGBA8 (RGBA or SRGB)");
		HY_CORE_ASSERT(pixels != nullptr, "VulkanImage: no pixels");

		m_width = spec.Width;
		m_height = spec.Height;
		m_format = GetImageFormat(spec.Format);
		m_vulkanImageInfo.size = m_width * m_height * 4;

		CreateImage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		UploadPixels(pixels, m_vulkanImageInfo.size);
		CreateViewAndSampler(VK_IMAGE_ASPECT_COLOR_BIT, properties);
	}

	VulkanImage::VulkanImage(ImageSpecification spec)
	{
		m_width = spec.Width;
		m_height = spec.Height;
		m_format = GetImageFormat(spec.Format);
		m_vulkanImageInfo.size = m_width * m_height * 4;

		bool isDepth = IsDepthFormat(spec.Format);

		VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		if (spec.Usage == ImageUsage::Attachment)
			usage |= isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else
			usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		CreateImage(usage);

		// Render targets are shown 1:1 (ImGui::Image), so clamp them.
		TextureProperties properties;
		properties.SamplerWrap = TextureWrap::Clamp;
		properties.SamplerFilter = TextureFilter::Linear;
		CreateViewAndSampler(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, properties);
	}

	VulkanImage::~VulkanImage()
	{
		VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		if (m_vulkanImageInfo.Sampler != VK_NULL_HANDLE)
			vkDestroySampler(device, m_vulkanImageInfo.Sampler, nullptr);

		if (m_vulkanImageInfo.ImageView != VK_NULL_HANDLE)
			vkDestroyImageView(device, m_vulkanImageInfo.ImageView, nullptr);

		if (m_vulkanImageInfo.Image != VK_NULL_HANDLE)
		{
			VulkanAllocator allocator("VulkanImage");
			allocator.DestroyImage(m_vulkanImageInfo.Image, m_vulkanImageInfo.MemoryAlloc);
		}
	}

	void VulkanImage::CreateImage(VkImageUsageFlags usage)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_width;
		imageInfo.extent.height = m_height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VulkanAllocator allocator("VulkanImage");
		m_vulkanImageInfo.MemoryAlloc = allocator.AllocateImage(imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, m_vulkanImageInfo.Image);
	}

	void VulkanImage::CreateViewAndSampler(VkImageAspectFlags aspect, const TextureProperties& properties)
	{
		VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_format;
		imageViewCreateInfo.subresourceRange.aspectMask = aspect;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.image = m_vulkanImageInfo.Image;

		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &m_vulkanImageInfo.ImageView));

		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		const VkFilter filter = GetFilter(properties.SamplerFilter);
		const VkSamplerAddressMode addressMode = GetAddressMode(properties.SamplerWrap);
		samplerCreateInfo.magFilter = filter;
		samplerCreateInfo.minFilter = filter;
		samplerCreateInfo.mipmapMode = filter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = addressMode;
		samplerCreateInfo.addressModeV = addressMode;
		samplerCreateInfo.addressModeW = addressMode;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_vulkanImageInfo.Sampler));
	}

	void VulkanImage::UploadPixels(const void* pixels, VkDeviceSize size)
	{
		VulkanAllocator allocator("VulkanImageStaging");

		VkBufferCreateInfo stagingInfo{};
		stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingInfo.size = size;
		stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAlloc = allocator.AllocateBuffer(stagingInfo, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);

		void* mapped = allocator.MapMemory<void>(stagingAlloc);
		memcpy(mapped, pixels, (size_t)size);
		allocator.UnmapMemory(stagingAlloc);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_vulkanImageInfo.Image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VulkanCommandBuffer upload;
		VkCommandBuffer commandBuffer = upload.Create();
		upload.Begin();

		// UNDEFINED -> TRANSFER_DST
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = { m_width, m_height, 1 };
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_vulkanImageInfo.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		// TRANSFER_DST -> SHADER_READ_ONLY
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		upload.End(); // Submits and waits, so the staging buffer can be freed right after.

		allocator.DestroyBuffer(stagingBuffer, stagingAlloc);
	}

	VkFormat VulkanImage::GetImageFormat(ImageFormat imageformat)
	{
		switch (imageformat)
		{
			case ImageFormat::RGBA:            return VK_FORMAT_R8G8B8A8_UNORM;
			case ImageFormat::SRGB:            return VK_FORMAT_R8G8B8A8_SRGB;
			case ImageFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
			case ImageFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
			case ImageFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
			case ImageFormat::DEPTH32F:        return VK_FORMAT_D32_SFLOAT;
			case ImageFormat::DEPTH24STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
			default:                           return VK_FORMAT_R8G8B8A8_UNORM;
		}
	}

	bool VulkanImage::IsDepthFormat(ImageFormat imageformat)
	{
		return imageformat == ImageFormat::DEPTH32F || imageformat == ImageFormat::DEPTH24STENCIL8;
	}

	VkFilter VulkanImage::GetFilter(TextureFilter filter)
	{
		switch (filter)
		{
			case TextureFilter::Nearest: return VK_FILTER_NEAREST;
			default:                     return VK_FILTER_LINEAR;
		}
	}

	VkSamplerAddressMode VulkanImage::GetAddressMode(TextureWrap wrap)
	{
		switch (wrap)
		{
			case TextureWrap::Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			default:                 return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}
}
