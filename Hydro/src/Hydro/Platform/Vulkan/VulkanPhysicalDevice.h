#pragma once
#include <vulkan/vulkan.h>

namespace Hydro
{
	struct QueueFamilyIndices
	{
		int32_t graphics = -1;
		int32_t present = -1;
	};

	class VulkanPhysicalDevice
	{
	public:
		VulkanPhysicalDevice(VkInstance instance);
		VkPhysicalDevice GetVulkanPhysicalDevice() { return m_PhysicalDevice; }
		QueueFamilyIndices GetQueueFamilyIndices() { return m_QueueFamilyIndices; }

	private:
		bool IsDeviceSuitable(VkPhysicalDevice device);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

	private:
		VkPhysicalDevice m_PhysicalDevice = nullptr;
		VkPhysicalDeviceProperties m_Properties;
		VkPhysicalDeviceFeatures m_Features;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;

		QueueFamilyIndices m_QueueFamilyIndices;

		uint32_t deviceCount = 0;
	};
}


