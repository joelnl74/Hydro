#include "hypch.h"

#include <optional>

#include "VulkanDevice.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

namespace Hydro
{
	VulkanPhysicalDevice::VulkanPhysicalDevice(VkInstance instance)
	{
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			HY_CORE_WARN("NO PHYSCIAL DEVICE FOUND THAT CAN HANDLE VULKAN!");

			// TODO EXIT ENGINE OR FALLBACK.
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) 
		{
			if (IsDeviceSuitable(device)) 
			{
				m_PhysicalDevice = device;
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);
	}

	bool VulkanPhysicalDevice::IsDeviceSuitable(VkPhysicalDevice device)
	{
		vkGetPhysicalDeviceProperties(device, &m_Properties);
		vkGetPhysicalDeviceFeatures(device, &m_Features);

		HY_CORE_INFO("Physical device selected: {0}", m_Properties.deviceName);

		return m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			m_Features.geometryShader;
	}

	QueueFamilyIndices VulkanPhysicalDevice::FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		std::optional<uint32_t> graphicsFamily;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		graphicsFamily = 0;

		for (int i = 0; i < queueFamilies.size(); i++)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
			{
				indices.Graphics = i;
				break;
			}
		}

		return indices;
	}

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

		VkResult result = vkCreateDevice(m_physicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &m_LogicalDevice);

		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("Creating logical device failed");
		}

		vkGetDeviceQueue(m_LogicalDevice, m_physicalDevice->GetQueueFamilyIndices().Graphics, 0, &m_GraphicsQueue);
	}
}

