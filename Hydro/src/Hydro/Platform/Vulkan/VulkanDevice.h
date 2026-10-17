#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanPhysicalDevice.h"

namespace Hydro
{
	class VulkanDevice
	{
	public:
		VulkanDevice(Ref<VulkanPhysicalDevice> physicalDevice);

		Ref<VulkanPhysicalDevice> GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetDevice() const { return m_LogicalDevice; }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

		// Pool for short-lived, one-time-submit command buffers (uploads, layout transitions).
		VkCommandPool GetUploadCommandPool() const { return m_UploadCommandPool; }

		bool IsSamplerAnisotropyEnabled() const { return m_SamplerAnisotropyEnabled; }
		bool IsFillModeNonSolidEnabled() const { return m_FillModeNonSolidEnabled; }
		bool IsWideLinesEnabled() const { return m_WideLinesEnabled; }

		void WaitIdle() const;
		void Shutdown();

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;

		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;

		VkCommandPool m_UploadCommandPool = VK_NULL_HANDLE;
		bool m_SamplerAnisotropyEnabled = false;
		bool m_FillModeNonSolidEnabled = false;
		bool m_WideLinesEnabled = false;

		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
	};
}
