#pragma once
#include <vulkan/vulkan.h>
#include "vulkanImage.h"
#include <vector>

namespace Hydro
{
	// Offscreen color target: one image + framebuffer per frame in flight, so the frame
	// being rendered never writes the image the previous frame's ImGui pass is still sampling.
	class VulkanRenderPass
	{
	public:
		VulkanRenderPass(uint32_t width, uint32_t height);

		// Recreates the images/framebuffers at a new size. The GPU must be idle
		// (or at least done with every frame in flight) before calling this.
		void Resize(uint32_t width, uint32_t height);
		void Destroy();

		VkFramebuffer GetFramebuffer(uint32_t frame) const { return m_Framebuffers[frame]; }
		VkRenderPass GetRenderPass() const { return m_renderPass; }
		const VulkanImage& GetImage(uint32_t frame) const { return m_images[frame]; }

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:
		void CreateRenderPass();
		void CreateTargets();
		void DestroyTargets();

	private:
		VkRenderPass m_renderPass = VK_NULL_HANDLE;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<VkFramebuffer> m_Framebuffers;
		std::vector<VulkanImage> m_images;
	};
}
