#include "hypch.h"

#include <optional>

#include "VulkanDevice.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

namespace Hydro
{
	VulkanDevice::VulkanDevice(VkInstance instance)
	{
		// Do we need to enable any other extensions (eg. NV_RAYTRACING?)
		std::vector<const char*> deviceExtensions;
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		m_physicalDevice = CreateRef<VulkanPhysicalDevice>(instance);

		VkDeviceCreateInfo deviceCreateInfo{};
		VkDeviceQueueCreateInfo queueCreateInfo{};
		VkPhysicalDeviceFeatures deviceFeatures{};

		// Device features
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		// Create Queue Info.
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = m_physicalDevice->GetQueueFamilyIndices().graphics;
		queueCreateInfo.queueCount = 1;

		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		// Create Device info.
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		VkResult result = vkCreateDevice(m_physicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &s_LogicalDevice);

		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("Creating logical device failed");
		}

		vkGetDeviceQueue(s_LogicalDevice, m_physicalDevice->GetQueueFamilyIndices().graphics, 0, &m_GraphicsQueue);
	}

	void VulkanDevice::ShutDown()
	{
		vkDestroyDevice(s_LogicalDevice, nullptr);
	}
}

