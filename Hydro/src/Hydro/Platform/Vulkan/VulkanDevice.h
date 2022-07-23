#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Platform//Vulkan/VulkanPhysicalDevice.h"

namespace Hydro
{
	class VulkanDevice
	{
	public:
		VulkanDevice(VkInstance instance);
		void ShutDown();

	private:
		static inline VkDevice s_LogicalDevice;
		
		VkQueue m_GraphicsQueue;
		Scope<VulkanPhysicalDevice> m_physicalDevice;
	};
}
