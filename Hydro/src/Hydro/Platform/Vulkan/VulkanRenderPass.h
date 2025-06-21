#pragma once
#include <vulkan/vulkan.h>
#include "vulkanImage.h"
#include <vector>

namespace Hydro
{
	class VulkanRenderPass
	{
	public:
		VulkanRenderPass();

		void CreateFrameBuffer();
		void CreateRenderPass();
		void CreateImage();

		VkFramebuffer GetFramebuffer() const { return m_Framebuffer[0]; }
		VkRenderPass GetRenderPass() const { return m_renderPass; }
		VulkanImage GetImage() const { return m_image[0]; }

	private:
		VkRenderPass m_renderPass = nullptr;
		
		std::vector<VkFramebuffer> m_Framebuffer;
		std::vector<VulkanImage> m_image;
	};
}


