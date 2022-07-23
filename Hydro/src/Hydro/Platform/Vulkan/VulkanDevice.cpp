#include "hypch.h"

#include <optional>

#include "VulkanDevice.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

namespace Hydro
{
	VulkanDevice::VulkanDevice(VkInstance instance)
	{
		m_physicalDevice = CreateScope<VulkanPhysicalDevice>(instance);

		VkDeviceCreateInfo deviceCreateInfo{};
		VkDeviceQueueCreateInfo queueCreateInfo{};
		VkPhysicalDeviceFeatures deviceFeatures{};

		// Create Queue Info.
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = m_physicalDevice->GetQueueFamilyIndices().Graphics;
		queueCreateInfo.queueCount = 1;

		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		//

		// Create Device info.
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		VkResult result = vkCreateDevice(m_physicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &s_LogicalDevice);

		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("Creating logical device failed");
		}

		vkGetDeviceQueue(s_LogicalDevice, m_physicalDevice->GetQueueFamilyIndices().Graphics, 0, &m_GraphicsQueue);
	}

	void VulkanDevice::ShutDown()
	{
		vkDestroyDevice(s_LogicalDevice, nullptr);
	}
}

