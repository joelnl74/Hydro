#include "hypch.h"
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
			if (isDeviceSuitable(device)) 
			{
				s_physicalDevice = device;
				break;
			}
		}

		if (s_physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	bool VulkanPhysicalDevice::isDeviceSuitable(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		HY_CORE_INFO("Physical device selected: {0}", deviceProperties.deviceName);


		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			deviceFeatures.geometryShader;
	}
}

