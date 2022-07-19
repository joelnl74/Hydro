#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Renderer/RendererContext.h"

namespace Hydro
{
	class VulkanRendererContext : public RendererContext
	{
	public:
		virtual ~VulkanRendererContext();

		virtual void Init() override;

	public:
		static VkInstance GetInstance() { return s_VulkanInstance; }

	private:
		// Debug messaging.
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		bool SetupDebugMessenger();

		// Validation Layers.
		bool CheckValidationLayerSupport();

	private:
		inline static VkInstance s_VulkanInstance;
		VkDebugUtilsMessengerEXT m_debugMessenger;
	};
}


