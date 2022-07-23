#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Platform//Vulkan/VulkanPhysicalDevice.h"

namespace Hydro
{
	class VulkanDevice
	{
	public:
		VulkanDevice(VkInstance instance);
		Ref<VulkanPhysicalDevice> GetPhysicalDevice() { return m_physicalDevice;  }
		VkDevice GetDevice() { return s_LogicalDevice; }

		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() { return m_presentQueue; }

		void ShutDown();

	private:
		static inline VkDevice s_LogicalDevice;
		
		VkQueue m_GraphicsQueue;
		VkQueue m_presentQueue;

		Ref<VulkanPhysicalDevice> m_physicalDevice;
	};
}
