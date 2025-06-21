#pragma once
#include "hypch.h"
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanRenderCommandBuffer
	{
	public:
		VulkanRenderCommandBuffer(uint32_t count = 0, const std::string& debugName = "");
		VulkanRenderCommandBuffer(const std::string& debugName = "");

		void Begin();
		void End();
		void Submit();

		VkCommandBuffer GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }

	private:
		std::string m_DebugName;
		VkCommandPool m_CommandPool = nullptr;
		std::vector<VkCommandBuffer> m_CommandBuffers;
		VkCommandBuffer m_ActiveCommandBuffer = nullptr;
		std::vector<VkFence> m_WaitFences;

		bool m_OwnedBySwapChain = false;
	};
}


