#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanDevice.h"

#include <array>

namespace Hydro
{
	constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	// Everything that exists once per frame in flight: the command buffer the frame is recorded into,
	// the fence that says the GPU is done with it, and the semaphore signalled when a swapchain image
	// has been acquired for it.
	class VulkanFrameContext
	{
	public:
		void Init(Ref<VulkanDevice> device);
		void Shutdown();

		// Blocks until the GPU has finished the last submission that used the current frame's resources.
		void WaitForFrame();
		// Only reset the fence once we are sure this frame will be submitted (after a successful acquire).
		void ResetFence();

		VkCommandBuffer BeginCommandBuffer();
		void EndCommandBuffer();

		// Waits on the image-acquired semaphore, signals `renderFinished` and the frame fence.
		void Submit(VkQueue queue, VkSemaphore renderFinished);

		void Advance() { m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT; }

		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		VkCommandBuffer GetCommandBuffer() const { return m_Frames[m_FrameIndex].CommandBuffer; }
		VkSemaphore GetImageAvailableSemaphore() const { return m_Frames[m_FrameIndex].ImageAvailable; }

	private:
		struct FrameData
		{
			VkCommandPool CommandPool = VK_NULL_HANDLE;
			VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
			VkFence InFlightFence = VK_NULL_HANDLE;
			VkSemaphore ImageAvailable = VK_NULL_HANDLE;
		};

		std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames{};
		uint32_t m_FrameIndex = 0;

		Ref<VulkanDevice> m_Device;
	};
}
