#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Platform/Vulkan/VulkanDevice.h"
#include "Hydro/Core/Window.h"

namespace Hydro
{
	// Owns the Vulkan instance, debug messenger, window surface and device. A part of the Renderer,
	// which starts and stops it; reach it with Renderer::GetRendererContext(), and the device with
	// Renderer::GetVulkanDevice().
	class VulkanRendererContext
	{
	public:
		VulkanRendererContext() = default;
		~VulkanRendererContext() = default;

		void Init(Window& window);
		void Shutdown();

	public:
		VkInstance GetInstance() const { return m_Instance; }
		// Valid between Init and Shutdown.
		VulkanDevice& GetVulkanDevice() { return *m_Device; }
		const VulkanDevice& GetVulkanDevice() const { return *m_Device; }
		bool HasVulkanDevice() const { return m_Device != nullptr; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }

	private:
		void CreateVulkanInstance();
		void CreateSurface(Window& window);

		// Debug messaging.
		static VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo();
		void SetupDebugMessenger();
		void DestroyDebugMessenger();

		// Validation layers and instance extensions.
		static bool CheckValidationLayerSupport();
		static bool HasInstanceExtension(const char* name);

	private:
		VkInstance m_Instance = VK_NULL_HANDLE;
		Scope<VulkanDevice> m_Device;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		bool m_ValidationActive = false;
	};
}
