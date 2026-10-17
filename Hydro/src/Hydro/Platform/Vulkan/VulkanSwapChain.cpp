#include "hypch.h"
#include "VulkanSwapChain.h"

#include "VulkanUtils.h"

#include <limits>

namespace Hydro
{
	VulkanSwapChain::VulkanSwapChain(Ref<VulkanDevice> device, VkSurfaceKHR surface)
		: m_Device(device), m_Surface(surface)
	{
	}

	bool VulkanSwapChain::Create(uint32_t width, uint32_t height, bool vsync)
	{
		m_VSync = vsync;

		SwapChainSupportDetails support = QuerySwapChainSupport();
		if (support.formats.empty() || support.presentModes.empty())
		{
			throw std::runtime_error("Surface has no formats or present modes.");
		}

		m_SurfaceFormat = ChooseSwapSurfaceFormat(support.formats);
		CreateRenderPass();

		return CreateSwapChainResources(width, height, VK_NULL_HANDLE);
	}

	bool VulkanSwapChain::Recreate(uint32_t width, uint32_t height)
	{
		// Build the new swapchain from the old one (lets the driver reuse resources), then free the old.
		VkSwapchainKHR oldSwapChain = m_SwapChain;
		std::vector<VkImageView> oldViews = std::move(m_ImageViews);
		std::vector<VkFramebuffer> oldFramebuffers = std::move(m_Framebuffers);
		std::vector<VkSemaphore> oldSemaphores = std::move(m_RenderFinishedSemaphores);
		m_ImageViews.clear();
		m_Framebuffers.clear();
		m_RenderFinishedSemaphores.clear();
		m_Images.clear();
		m_SwapChain = VK_NULL_HANDLE;

		bool created = CreateSwapChainResources(width, height, oldSwapChain);

		VkDevice device = m_Device->GetDevice();
		for (VkFramebuffer framebuffer : oldFramebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		for (VkImageView view : oldViews)
			vkDestroyImageView(device, view, nullptr);
		for (VkSemaphore semaphore : oldSemaphores)
			vkDestroySemaphore(device, semaphore, nullptr);
		if (oldSwapChain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(device, oldSwapChain, nullptr);

		return created;
	}

	void VulkanSwapChain::Shutdown()
	{
		DestroySwapChainResources();

		if (m_RenderPass != VK_NULL_HANDLE)
		{
			vkDestroyRenderPass(m_Device->GetDevice(), m_RenderPass, nullptr);
			m_RenderPass = VK_NULL_HANDLE;
		}
	}

	bool VulkanSwapChain::CreateSwapChainResources(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapChain)
	{
		VkDevice device = m_Device->GetDevice();
		SwapChainSupportDetails support = QuerySwapChainSupport();

		VkExtent2D extent = ChooseSwapExtent(support.capabilities, width, height);
		if (extent.width == 0 || extent.height == 0)
		{
			// Minimised: there is nothing to present to until the window is restored.
			m_Extent = extent;
			return false;
		}

		uint32_t imageCount = support.capabilities.minImageCount + 1;
		if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
		{
			imageCount = support.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = m_SurfaceFormat.format;
		createInfo.imageColorSpace = m_SurfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		const QueueFamilyIndices& families = m_Device->GetPhysicalDevice()->GetQueueFamilyIndices();
		uint32_t familyIndices[] = { (uint32_t)families.graphics, (uint32_t)families.present };

		if (families.graphics != families.present)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = familyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = support.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = ChooseSwapPresentMode(support.presentModes, m_VSync);
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapChain;

		VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_SwapChain);
		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("vkCreateSwapchainKHR failed: {0}", VKResultToString(result));
			throw std::runtime_error("Failed to create swap chain.");
		}

		m_Extent = extent;
		m_MinImageCount = imageCount;

		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, m_Images.data());

		m_ImageViews.resize(imageCount);
		m_Framebuffers.resize(imageCount);
		m_RenderFinishedSemaphores.resize(imageCount);

		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_Images[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_SurfaceFormat.format;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageViews[i]));

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &m_ImageViews[i];
			framebufferInfo.width = m_Extent.width;
			framebufferInfo.height = m_Extent.height;
			framebufferInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]));

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));
		}

		return true;
	}

	void VulkanSwapChain::DestroySwapChainResources()
	{
		VkDevice device = m_Device->GetDevice();

		for (VkFramebuffer framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		for (VkImageView view : m_ImageViews)
			vkDestroyImageView(device, view, nullptr);
		for (VkSemaphore semaphore : m_RenderFinishedSemaphores)
			vkDestroySemaphore(device, semaphore, nullptr);

		m_Framebuffers.clear();
		m_ImageViews.clear();
		m_RenderFinishedSemaphores.clear();
		m_Images.clear(); // Owned by the swapchain.

		if (m_SwapChain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
			m_SwapChain = VK_NULL_HANDLE;
		}
	}

	void VulkanSwapChain::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_SurfaceFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		// The image-acquired semaphore is waited on at COLOR_ATTACHMENT_OUTPUT, so the layout
		// transition and the clear must not start before that stage.
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(m_Device->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass));
	}

	VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore imageAvailable, uint32_t& outImageIndex)
	{
		return vkAcquireNextImageKHR(m_Device->GetDevice(), m_SwapChain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &outImageIndex);
	}

	VkResult VulkanSwapChain::Present(uint32_t imageIndex)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.pImageIndices = &imageIndex;

		return vkQueuePresentKHR(m_Device->GetPresentQueue(), &presentInfo);
	}

	void VulkanSwapChain::BeginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue clearColor{};
		clearColor.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = m_Framebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_Extent;
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanSwapChain::EndRenderPass(VkCommandBuffer commandBuffer)
	{
		vkCmdEndRenderPass(commandBuffer);
	}

	SwapChainSupportDetails VulkanSwapChain::QuerySwapChainSupport() const
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &details.capabilities);

		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync)
	{
		// FIFO is always available and is true vsync (capped to the display rate).
		if (vsync)
			return VK_PRESENT_MODE_FIFO_KHR;

		// Without vsync prefer mailbox (no tearing, uncapped), then immediate (may tear).
		for (VkPresentModeKHR mode : availablePresentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}

		for (VkPresentModeKHR mode : availablePresentModes)
		{
			if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				return mode;
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}

		VkExtent2D actualExtent = { width, height };
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}
}
