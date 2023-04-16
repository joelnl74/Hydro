#pragma once
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanCommandBuffer
	{
	public:
		VulkanCommandBuffer() {};
		
		VkCommandBuffer Create();
		void Begin();
		void End();

	private:
		VkCommandBuffer m_commandBuffer;
	};

}

