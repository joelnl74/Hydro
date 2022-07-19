#pragma once
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanPhysicalDevice
	{

	public:
		VulkanPhysicalDevice(VkInstance instance);
		static VkPhysicalDevice GetInstance() { return s_physicalDevice; }

	private:
		bool isDeviceSuitable(VkPhysicalDevice device);

	private:
		inline static VkPhysicalDevice s_physicalDevice = VK_NULL_HANDLE;
		uint32_t deviceCount = 0;

	};
}
