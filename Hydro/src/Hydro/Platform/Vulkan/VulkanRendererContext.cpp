#include "hypch.h"

#include <iostream>

#include "VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Hydro
{
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"

#ifdef HY_DEBUG
	const bool validationEnabled = true;
#else
	const bool validationEnabled = false;
#endif

	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		HY_CORE_WARN("VulkanDebugCallback:\n  Object Type: {0}\n  Message: {1}", messageType, pCallbackData->pMessage);

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	VulkanRendererContext::VulkanRendererContext()
	{
	}

	VulkanRendererContext::~VulkanRendererContext()
	{
		if (validationEnabled) 
		{
			DestroyDebugUtilsMessengerEXT(s_VulkanInstance, m_debugMessenger, nullptr);
		}

		s_VulkanDevice->ShutDown();

		vkDestroyInstance(s_VulkanInstance, nullptr);

		s_VulkanDevice = nullptr;
		s_VulkanInstance = nullptr;
	}

	void VulkanRendererContext::Init()
	{
		CreateVulkanInstance();

		s_VulkanDevice = CreateRef<VulkanDevice>(s_VulkanInstance);
	}

	void VulkanRendererContext::CreateVulkanInstance()
	{
		HY_CORE_INFO("Start Creating VulkanContext");

		bool supportVulkan = glfwVulkanSupported();

		HY_CORE_INFO("VULKAN SUPPORT {}", supportVulkan);

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hydro";
		appInfo.pEngineName = "Hydro";
		appInfo.apiVersion = VK_API_VERSION_1_2;

		std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
		if (validationEnabled)
		{
			instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
		instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;

		if (CheckValidationLayerSupport())
		{
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		}
		else
		{
			HY_CORE_ERROR("Validation layer VK_LAYER_LUNARG_standard_validation not present, validation is disabled");
		}

		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_VulkanInstance))

			if (validationEnabled)
			{
				SetupDebugMessenger();
			}
	}

	VkResult VulkanRendererContext::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void VulkanRendererContext::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	bool VulkanRendererContext::CheckValidationLayerSupport()
	{
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		bool validationLayerPresent = false;

		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

		HY_CORE_TRACE("Vulkan Instance Layers:");
		
		for (const VkLayerProperties& layer : instanceLayerProperties)
		{
			HY_CORE_TRACE("  {0}", layer.layerName);
			if (strcmp(layer.layerName, validationLayerName) == 0)
			{
				validationLayerPresent = true;
				break;
			}
		}

		return validationLayerPresent;
	}

	bool VulkanRendererContext::SetupDebugMessenger()
	{
		HY_CORE_INFO("Created Vulkan Context Succesfully");

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr; // Optional

		VK_SUCCESS(CreateDebugUtilsMessengerEXT(s_VulkanInstance, &createInfo, nullptr, &m_debugMessenger));

		HY_CORE_INFO("VULKAN Debug messenger setup correctly");

		return true;
	}
}
