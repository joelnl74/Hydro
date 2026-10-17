#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "VulkanImage.h"

#include <vector>

namespace Hydro
{
	// Offscreen colour target that is rendered to and then sampled (e.g. by the editor viewport).
	// Holds one image + framebuffer per frame in flight, so a frame never writes the image that an
	// earlier, still-running frame is reading.
	class VulkanRenderPass
	{
	public:
		VulkanRenderPass(uint32_t width, uint32_t height, ImageFormat format = ImageFormat::SRGB);
		~VulkanRenderPass();

		VulkanRenderPass(const VulkanRenderPass&) = delete;
		VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

		// Rebuilds the images and framebuffers; the VkRenderPass (and pipelines made with it) stay valid.
		// The caller must make sure the GPU is no longer using the old images.
		void Resize(uint32_t width, uint32_t height);

		void Begin(VkCommandBuffer commandBuffer, uint32_t frameIndex);
		void End(VkCommandBuffer commandBuffer);

		VkRenderPass GetRenderPass() const { return m_renderPass; }
		VkFramebuffer GetFramebuffer(uint32_t frameIndex) const { return m_Framebuffers[frameIndex]; }
		const VulkanImage& GetImage(uint32_t frameIndex) const { return *m_Images[frameIndex]; }
		VkExtent2D GetExtent() const { return { m_Width, m_Height }; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

	private:
		void CreateRenderPass();
		void CreateTargets();
		void DestroyTargets();

	private:
		VkRenderPass m_renderPass = VK_NULL_HANDLE;
		ImageFormat m_Format;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		std::vector<Scope<VulkanImage>> m_Images;
		std::vector<VkFramebuffer> m_Framebuffers;
	};
}
