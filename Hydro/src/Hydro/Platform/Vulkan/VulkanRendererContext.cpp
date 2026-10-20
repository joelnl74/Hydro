#include "hypch.h"

#include "VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstring>

namespace Hydro
{
#ifdef HY_DEBUG
	static constexpr bool s_ValidationRequested = true;
#else
	static constexpr bool s_ValidationRequested = false;
#endif

	static const char* s_ValidationLayerName = "VK_LAYER_KHRONOS_validation";

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		[[maybe_unused]] void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			HY_CORE_ERROR("Vulkan [type {0}]: {1}", messageType, pCallbackData->pMessage);
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			HY_CORE_WARN("Vulkan [type {0}]: {1}", messageType, pCallbackData->pMessage);
		}
		else
		{
			HY_CORE_TRACE("Vulkan [type {0}]: {1}", messageType, pCallbackData->pMessage);
		}

		return VK_FALSE;
	}

	void VulkanRendererContext::Init(Window& window)
	{
		HY_CORE_ASSERT(m_Instance == VK_NULL_HANDLE, "VulkanRendererContext::Init called twice");

		CreateVulkanInstance();
		CreateSurface(window);

		// The surface must exist before the GPU is picked, so we only pick GPUs that can present to it.
		m_Device = CreateScope<VulkanDevice>(CreateScope<VulkanPhysicalDevice>(m_Instance, m_Surface));
	}

	void VulkanRendererContext::Shutdown()
	{
		if (m_Instance == VK_NULL_HANDLE)
		{
			return;
		}

		if (m_Device != nullptr)
		{
			m_Device->Shutdown();
			m_Device.reset();
		}

		if (m_Surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
			m_Surface = VK_NULL_HANDLE;
		}

		DestroyDebugMessenger();

		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = VK_NULL_HANDLE;
	}

	void VulkanRendererContext::CreateVulkanInstance()
	{
		HY_CORE_INFO("Creating Vulkan instance");

		if (glfwVulkanSupported() == GLFW_FALSE)
		{
			throw std::runtime_error("GLFW reports that Vulkan is not supported on this system.");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hydro";
		appInfo.pEngineName = "Hydro";
		appInfo.apiVersion = VK_API_VERSION_1_2;

		// Start from what GLFW needs for the window surface (VK_KHR_surface + the platform surface),
		// then add the debug-utils extension when validation is on.
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		m_ValidationActive = false;
		if (s_ValidationRequested)
		{
			if (CheckValidationLayerSupport())
			{
				m_ValidationActive = true;
				instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
			else
			{
				HY_CORE_WARN("Validation layer {0} not present, validation is disabled", s_ValidationLayerName);
			}
		}

		// macOS: Vulkan runs on Metal through a portability driver (MoltenVK or KosmicKrisp), which the
		// loader only lists when the instance asks for portability devices. Harmless elsewhere.
		const bool portabilityEnumeration = HasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		if (portabilityEnumeration)
		{
			instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		}

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		if (portabilityEnumeration)
		{
			instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}
		instanceCreateInfo.pApplicationInfo = &appInfo;
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

		// Chaining a messenger create-info also reports problems inside vkCreateInstance/vkDestroyInstance.
		VkDebugUtilsMessengerCreateInfoEXT instanceDebugInfo = MakeDebugMessengerCreateInfo();
		if (m_ValidationActive)
		{
			instanceCreateInfo.enabledLayerCount = 1;
			instanceCreateInfo.ppEnabledLayerNames = &s_ValidationLayerName;
			instanceCreateInfo.pNext = &instanceDebugInfo;
		}

		VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance);
		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("vkCreateInstance failed: {0}", VKResultToString(result));
			throw std::runtime_error("Failed to create Vulkan instance.");
		}

		if (m_ValidationActive)
		{
			SetupDebugMessenger();
		}
	}

	void VulkanRendererContext::CreateSurface(Window& window)
	{
		GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		VkResult result = glfwCreateWindowSurface(m_Instance, glfwWindow, nullptr, &m_Surface);
		if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("glfwCreateWindowSurface failed: {0}", VKResultToString(result));
			throw std::runtime_error("Failed to create window surface.");
		}
	}

	VkDebugUtilsMessengerCreateInfoEXT VulkanRendererContext::MakeDebugMessengerCreateInfo()
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr;
		return createInfo;
	}

	void VulkanRendererContext::SetupDebugMessenger()
	{
		auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
		if (createFunc == nullptr)
		{
			HY_CORE_WARN("vkCreateDebugUtilsMessengerEXT not available, validation messages will not be logged");
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo = MakeDebugMessengerCreateInfo();
		VK_CHECK_RESULT(createFunc(m_Instance, &createInfo, nullptr, &m_DebugMessenger));

		HY_CORE_INFO("Vulkan debug messenger set up");
	}

	void VulkanRendererContext::DestroyDebugMessenger()
	{
		if (m_DebugMessenger == VK_NULL_HANDLE)
		{
			return;
		}

		auto destroyFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroyFunc != nullptr)
		{
			destroyFunc(m_Instance, m_DebugMessenger, nullptr);
		}

		m_DebugMessenger = VK_NULL_HANDLE;
	}

	bool VulkanRendererContext::HasInstanceExtension(const char* name)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		for (const VkExtensionProperties& extension : extensions)
		{
			if (strcmp(extension.extensionName, name) == 0)
			{
				return true;
			}
		}

		return false;
	}

	bool VulkanRendererContext::CheckValidationLayerSupport()
	{
		uint32_t instanceLayerCount = 0;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);

		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

		for (const VkLayerProperties& layer : instanceLayerProperties)
		{
			if (strcmp(layer.layerName, s_ValidationLayerName) == 0)
			{
				return true;
			}
		}

		return false;
	}
}
