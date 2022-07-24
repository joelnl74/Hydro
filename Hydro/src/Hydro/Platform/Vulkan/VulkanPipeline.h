#pragma once
#include <vulkan/vulkan.h>

namespace Hydro
{
	class VulkanPipeline
	{
	public:
		void Create(VkExtent2D extents);

	private:
		VkPipelineLayout pipelineLayout;
		VkRenderPass renderPass;
	};
}


