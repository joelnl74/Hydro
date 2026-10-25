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
		std::string rejections; // every GPU that can't be used and why, for the error message

		for (VkPhysicalDevice device : devices)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(device, &properties);

			const std::string missing = FindMissingRequirement(device, surface);
			if (missing.empty() == false)
			{
				HY_CORE_WARN("GPU candidate: {0} (Vulkan {1}.{2}) can't be used: {3}", properties.deviceName,
					VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), missing);
				rejections += std::string("\n  ") + properties.deviceName + ": " + missing;
				continue;
			}

			const int score = ScoreDevice(device);
			HY_CORE_INFO("GPU candidate: {0} (Vulkan {1}.{2}, score {3})", properties.deviceName,
				VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), score);

			if (score > bestScore)
			{
				bestScore = score;
				m_PhysicalDevice = device;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			const std::string message = "No GPU can run Hydro: it needs Vulkan 1.3 (dynamic rendering and synchronization2) "
				"and must present to this window. Updating the graphics driver usually adds Vulkan 1.3." + rejections;
			HY_CORE_ERROR("{0}", message);
			throw std::runtime_error(message);
		}

		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_Properties);
		QueryFeatures(m_PhysicalDevice, m_Features, m_Features12, m_Features13);
		m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice, surface);

		HY_CORE_INFO("Physical device selected: {0}", m_Properties.deviceName);
	}

	void VulkanPhysicalDevice::QueryFeatures(VkPhysicalDevice device, VkPhysicalDeviceFeatures& features,
		VkPhysicalDeviceVulkan12Features& features12, VkPhysicalDeviceVulkan13Features& features13)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		features12 = {};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features13 = {};
		features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

		// A struct is only chained when the device has its version (the validation layers object to
		// a 1.3 struct on a 1.2 device). The 1.2 struct needs 1.2.
		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		if (properties.apiVersion >= VK_API_VERSION_1_2)
		{
			features2.pNext = &features12;
		}
		if (properties.apiVersion >= VK_API_VERSION_1_3)
		{
			features12.pNext = &features13;
		}

		vkGetPhysicalDeviceFeatures2(device, &features2);
		features = features2.features;

		// Not part of the result: the caller's copies must not point at this function's locals.
		features12.pNext = nullptr;
		features13.pNext = nullptr;
	}

	std::string VulkanPhysicalDevice::FindMissingRequirement(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		if (properties.apiVersion < HY_REQUIRED_VULKAN_VERSION)
		{
			return "Vulkan 1.3 is needed, the driver reports " + std::to_string(VK_API_VERSION_MAJOR(properties.apiVersion))
				+ "." + std::to_string(VK_API_VERSION_MINOR(properties.apiVersion));
		}

		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceVulkan12Features features12;
		VkPhysicalDeviceVulkan13Features features13;
		QueryFeatures(device, features, features12, features13);

		if (features13.dynamicRendering == VK_FALSE)
		{
			return "no dynamicRendering";
		}
		if (features13.synchronization2 == VK_FALSE)
		{
			return "no synchronization2";
		}

		// A graphics queue, a queue that can present, the swapchain extension and at least one
		// surface format and present mode.
		if (FindQueueFamilies(device, surface).IsComplete() == false)
		{
			return "no queue that can draw and present to this window";
		}
		if (SupportsRequiredExtensions(device) == false)
		{
			return "no VK_KHR_swapchain";
		}
		if (SupportsSurface(device, surface) == false)
		{
			return "no surface format or present mode for this window";
		}

		return {};
	}

	int VulkanPhysicalDevice::ScoreDevice(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		int score = 1;

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}
		else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		{
			score += 100;
		}

		return score;
	}

	bool VulkanPhysicalDevice::SupportsDepthAttachment(VkFormat format) const
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &properties);
		return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
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
