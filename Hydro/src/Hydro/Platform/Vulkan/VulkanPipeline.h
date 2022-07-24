#pragma once
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanPipeline
	{
	public:
		void Create(VkExtent2D extents, VkRenderPass renderpass, VkPipelineShaderStageCreateInfo stages[]);
		void ShutDown();

		VkPipelineLayout GetPipeLineLayout() { return m_PipelineLayout; }
		VkPipeline GetPipeLine() { return m_GraphicsPipeline;  }
		VkViewport GetViewPort() { return m_ViewPort; }
		VkRect2D GetRect2D() { return m_Scissor; }
	private:
		VkPipelineLayout m_PipelineLayout;
		VkViewport m_ViewPort;
		VkRect2D m_Scissor;
		VkPipeline m_GraphicsPipeline;

	};
}


