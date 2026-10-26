#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanPhysicalDevice.h"

#include <functional>

namespace Hydro
{
	class VulkanDevice
	{
	public:
		// Takes ownership of the GPU it was created for.
		explicit VulkanDevice(Scope<VulkanPhysicalDevice> physicalDevice);

		VulkanDevice(const VulkanDevice&) = delete;
		VulkanDevice& operator=(const VulkanDevice&) = delete;

		const VulkanPhysicalDevice& GetPhysicalDevice() const { return *m_PhysicalDevice; }
		VkDevice GetDevice() const { return m_LogicalDevice; }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

		// One-off GPU work outside the frame (uploads, layout transitions): records `record` into a
		// one-time command buffer from the device's upload pool, submits it to the graphics queue and
		// waits for it, so staging memory can be freed right after. Blocks: never use it for per-frame
		// work (record that into Renderer::GetCurrentCommandBuffer()). Main thread only.
		void SubmitImmediate(const std::function<void(VkCommandBuffer)>& record);

		// Enabled features (Vulkan 1.3 core: dynamic rendering, synchronization2; optional: the rest).
		bool IsDynamicRenderingEnabled() const { return m_DynamicRenderingEnabled; }
		bool IsSynchronization2Enabled() const { return m_Synchronization2Enabled; }
		bool IsSamplerAnisotropyEnabled() const { return m_SamplerAnisotropyEnabled; }
		bool IsFillModeNonSolidEnabled() const { return m_FillModeNonSolidEnabled; }
		bool IsWideLinesEnabled() const { return m_WideLinesEnabled; }
		bool IsIndependentBlendEnabled() const { return m_IndependentBlendEnabled; }

		void WaitIdle() const;
		void Shutdown();

	private:
		VkDevice m_LogicalDevice = VK_NULL_HANDLE;

		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;

		VkCommandPool m_UploadCommandPool = VK_NULL_HANDLE;
		bool m_DynamicRenderingEnabled = false;
		bool m_Synchronization2Enabled = false;
		bool m_SamplerAnisotropyEnabled = false;
		bool m_FillModeNonSolidEnabled = false;
		bool m_WideLinesEnabled = false;
		bool m_IndependentBlendEnabled = false;

		Scope<VulkanPhysicalDevice> m_PhysicalDevice;
	};
}
