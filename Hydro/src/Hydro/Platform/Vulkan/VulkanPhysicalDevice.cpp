#include "hypch.h"
#include "VulkanPhysicalDevice.h"

#include <cstring>

namespace Hydro
{
	const std::vector<const char*>& VulkanPhysicalDevice::GetRequiredExtensions()
	{
		static const std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		return extensions;
	}

	VulkanPhysicalDevice::VulkanPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error("No GPU with Vulkan support found.");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		int bestScore = 0;

		for (VkPhysicalDevice device : devices)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(device, &properties);

			int score = ScoreDevice(device, surface);
			HY_CORE_INFO("GPU candidate: {0} (score {1})", properties.deviceName, score);

			if (score > bestScore)
			{
				bestScore = score;
				m_PhysicalDevice = device;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("No GPU can render and present to this window.");
		}

		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_Properties);
		vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
		m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice, surface);

		HY_CORE_INFO("Physical device selected: {0}", m_Properties.deviceName);
	}

	int VulkanPhysicalDevice::ScoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		// Hard requirements: a graphics queue, a queue that can present, the swapchain extension
		// and at least one surface format and present mode.
		if (FindQueueFamilies(device, surface).IsComplete() == false)
			return 0;

		if (SupportsRequiredExtensions(device) == false)
			return 0;

		if (SupportsSurface(device, surface) == false)
			return 0;

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		int score = 1;

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score += 1000;
		else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score += 100;

		return score;
	}

	QueueFamilyIndices VulkanPhysicalDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount; i++)
		{
			bool supportsGraphics = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

			VkBool32 supportsPresent = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supportsPresent);

			// Prefer one family that does both, so the swapchain can use exclusive sharing.
			if (supportsGraphics && supportsPresent == VK_TRUE)
			{
				indices.graphics = (int32_t)i;
				indices.present = (int32_t)i;
				return indices;
			}

			if (supportsGraphics && indices.graphics < 0)
				indices.graphics = (int32_t)i;

			if (supportsPresent == VK_TRUE && indices.present < 0)
				indices.present = (int32_t)i;
		}

		return indices;
	}

	bool VulkanPhysicalDevice::SupportsRequiredExtensions(VkPhysicalDevice device)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> available(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

		for (const char* required : GetRequiredExtensions())
		{
			bool found = false;

			for (const VkExtensionProperties& extension : available)
			{
				if (strcmp(extension.extensionName, required) == 0)
				{
					found = true;
					break;
				}
			}

			if (found == false)
				return false;
		}

		return true;
	}

	bool VulkanPhysicalDevice::SupportsSurface(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		return formatCount > 0 && presentModeCount > 0;
	}
}
