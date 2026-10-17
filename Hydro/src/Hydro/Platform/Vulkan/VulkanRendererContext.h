#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Hydro/Platform/Vulkan/VulkanDevice.h"
#include "Hydro/Core/Window.h"

namespace Hydro
{
	// Owns the Vulkan instance, debug messenger, window surface and device.
	class VulkanRendererContext
	{
	public:
		VulkanRendererContext() = default;
		~VulkanRendererContext() = default;

		void Init(Window& window);
		void ShutDown();

	public:
		static VkInstance GetInstance() { return s_VulkanInstance; }
		static Ref<VulkanDevice> GetVulkanDevice() { return s_VulkanDevice; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }

	private:
		void CreateVulkanInstance();
		void CreateSurface(Window& window);

		// Debug messaging.
		static VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo();
		void SetupDebugMessenger();
		void DestroyDebugMessenger();

		// Validation layers.
		static bool CheckValidationLayerSupport();

	private:
		inline static VkInstance s_VulkanInstance = VK_NULL_HANDLE;
		inline static Ref<VulkanDevice> s_VulkanDevice;

		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		bool m_ValidationActive = false;
	};
}
