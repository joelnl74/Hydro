#pragma once
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Hydro
{
	struct QueueFamilyIndices
	{
		int32_t graphics = -1;
		int32_t present = -1;

		bool IsComplete() const { return graphics >= 0 && present >= 0; }
	};

	// The Vulkan version the engine needs: dynamic rendering and synchronization2 are core in 1.3.
	constexpr uint32_t HY_REQUIRED_VULKAN_VERSION = VK_API_VERSION_1_3;

	class VulkanPhysicalDevice
	{
	public:
		// Picks the best GPU that can render and present to the given surface and has Vulkan 1.3 with
		// dynamicRendering and synchronization2. Throws with a message naming what is missing.
		VulkanPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
		// What the GPU supports (queried once with vkGetPhysicalDeviceFeatures2). What is enabled is
		// VulkanDevice's business.
		const VkPhysicalDeviceFeatures& GetFeatures() const { return m_Features; }
		const VkPhysicalDeviceVulkan12Features& GetVulkan12Features() const { return m_Features12; }
		const VkPhysicalDeviceVulkan13Features& GetVulkan13Features() const { return m_Features13; }

		// Optimal-tiling support for `format` as a depth attachment.
		bool SupportsDepthAttachment(VkFormat format) const;

		static const std::vector<const char*>& GetRequiredExtensions();

	private:
		// The features of `device` in the three structs (the 1.3 struct only when the device has 1.3).
		static void QueryFeatures(VkPhysicalDevice device, VkPhysicalDeviceFeatures& features,
			VkPhysicalDeviceVulkan12Features& features12, VkPhysicalDeviceVulkan13Features& features13);
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
		static bool SupportsRequiredExtensions(VkPhysicalDevice device);
		static bool SupportsSurface(VkPhysicalDevice device, VkSurfaceKHR surface);
		// Empty when the device can be used, else why not (for the log and the error message).
		static std::string FindMissingRequirement(VkPhysicalDevice device, VkSurfaceKHR surface);
		static int ScoreDevice(VkPhysicalDevice device);

	private:
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_Properties{};
		VkPhysicalDeviceFeatures m_Features{};
		VkPhysicalDeviceVulkan12Features m_Features12{};
		VkPhysicalDeviceVulkan13Features m_Features13{};

		QueueFamilyIndices m_QueueFamilyIndices;
	};
}
