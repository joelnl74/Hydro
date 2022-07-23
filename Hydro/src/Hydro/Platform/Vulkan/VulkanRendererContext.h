#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Hydro/Renderer/RendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanDevice.h"
#include "Hydro/Core/Window.h"
namespace Hydro
{
	class VulkanRendererContext : public RendererContext
	{
	public:
		virtual ~VulkanRendererContext();

		virtual void Init(Window &window) override;

	public:
		static VkInstance GetInstance() { return s_VulkanInstance; }
		static VulkanDevice& const GetPhysicalDevice()  { return *s_VulkanDevice; }

	private:
		// Setup
		void CreateVulkanInstance();
		void CreateVulkanSurface(Window& window);

		// Debug messaging.
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		bool SetupDebugMessenger();

		// Validation Layers.
		bool CheckValidationLayerSupport();

	private:
		inline static VkInstance s_VulkanInstance;
		inline static VkSurfaceKHR s_vkSurface;

		inline static VulkanDevice *s_VulkanDevice;

		VkDebugUtilsMessengerEXT m_debugMessenger;
	};
}


