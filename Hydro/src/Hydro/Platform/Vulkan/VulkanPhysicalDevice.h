#pragma once
#include <vulkan/vulkan.h>

#include <vector>

namespace Hydro
{
	struct QueueFamilyIndices
	{
		int32_t graphics = -1;
		int32_t present = -1;

		bool IsComplete() const { return graphics >= 0 && present >= 0; }
	};

	class VulkanPhysicalDevice
	{
	public:
		// Picks the best GPU that can render and present to the given surface.
		VulkanPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
		const VkPhysicalDeviceFeatures& GetFeatures() const { return m_Features; }
		// Whether the GPU offers a device extension (required or optional).
		bool SupportsExtension(const char* name) const;

		static const std::vector<const char*>& GetRequiredExtensions();

	private:
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
		static bool SupportsRequiredExtensions(VkPhysicalDevice device);
		static bool SupportsSurface(VkPhysicalDevice device, VkSurfaceKHR surface);
		static int ScoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface);

	private:
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_Properties{};
		VkPhysicalDeviceFeatures m_Features{};
		std::vector<VkExtensionProperties> m_Extensions;

		QueueFamilyIndices m_QueueFamilyIndices;
	};
}
