#include "hypch.h"
#include "VulkanPresentation.h"
#include <set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "VulkanUtils.h"

namespace Hydro
{
	static PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
	static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
	static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
	static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
	static PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
	static PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
	static PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
	static PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
	static PFN_vkQueuePresentKHR fpQueuePresentKHR;

	void VulkanPresentation::Init(VkInstance instance, Ref<VulkanDevice> vulkanDevice)
	{
		m_Device = vulkanDevice;
		m_instance = instance;
	}
	void VulkanPresentation::InitSurface(Window& window)
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		auto glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(glfwWindow);
		createInfo.hinstance = GetModuleHandle(nullptr);

		if (vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &s_vkSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		if (glfwCreateWindowSurface(m_instance, glfwWindow, nullptr, &s_vkSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		// Get available queue family properties
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportsPresent(queueCount);

		uint32_t presentFamily = 0;

		for (uint32_t i = 0; i < queueCount; i++)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, s_vkSurface, &supportsPresent[i]);

			if (supportsPresent[i]) 
			{
				presentFamily = i;
				break;
			}
		}

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { (uint32_t)m_Device->GetPhysicalDevice()->GetQueueFamilyIndices().graphics, presentFamily};

		vkGetDeviceQueue(m_Device->GetDevice(), presentFamily, 0, &m_PresentQueue);
	}
	void VulkanPresentation::CreateSwapChain(Window& window, bool vsync)
	{
		VkDevice device = m_Device->GetDevice();
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		m_SwapChainSupportDetails = QuerySwapChainSupport();

		bool swapChainAdequate = false;
		swapChainAdequate = !m_SwapChainSupportDetails.formats.empty() && !m_SwapChainSupportDetails.presentModes.empty();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(m_SwapChainSupportDetails.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(m_SwapChainSupportDetails.presentModes, vsync);
		VkExtent2D extent = ChooseSwapExtent(m_SwapChainSupportDetails.capabilities, window);

		uint32_t imageCount = m_SwapChainSupportDetails.capabilities.minImageCount + 1;

		if (m_SwapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > m_SwapChainSupportDetails.capabilities.maxImageCount)
		{
			imageCount = m_SwapChainSupportDetails.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = s_vkSurface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// TODO Check this with the queue family.
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		createInfo.preTransform = m_SwapChainSupportDetails.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;


		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;

		CreateImageViews();
	}

	void VulkanPresentation::ShutDown()
	{
		auto device = m_Device->GetDevice();

		for (auto imageView : m_SwapChainImageViews) 
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_Device->GetDevice(), m_SwapChain, nullptr);
		vkDestroySurfaceKHR(m_instance, s_vkSurface, nullptr);
	}

	SwapChainSupportDetails VulkanPresentation::QuerySwapChainSupport()
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, s_vkSurface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, s_vkSurface, &formatCount, nullptr);

		if (formatCount != 0) 
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, s_vkSurface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, s_vkSurface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, s_vkSurface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}
	VkSurfaceFormatKHR VulkanPresentation::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR VulkanPresentation::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync)
	{
		if (vsync)
		{
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanPresentation::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window)
	{
		auto glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
		{
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(glfwWindow, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void VulkanPresentation::CreateImageViews()
	{
		auto device = m_Device->GetDevice();

		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i = 0; i < m_SwapChainImages.size(); i++) 
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_SwapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_SwapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &m_SwapChainImageViews[i]));
		}
	}
}
