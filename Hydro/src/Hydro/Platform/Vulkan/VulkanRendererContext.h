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
		VulkanRendererContext();
		virtual ~VulkanRendererContext();

		virtual void Init() override;

	public:
		static VkInstance GetInstance() { return s_VulkanInstance; }
		static Ref<VulkanDevice> & const GetVulkanDevice()  { return s_VulkanDevice; }

	private:
		// Setup
		void CreateVulkanInstance();

		// Debug messaging.
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		bool SetupDebugMessenger();

		// Validation Layers.
		bool CheckValidationLayerSupport();

	private:
		inline static VkInstance s_VulkanInstance;
		inline static Ref<VulkanDevice> s_VulkanDevice;

		VkDebugUtilsMessengerEXT m_debugMessenger;
	};
}


