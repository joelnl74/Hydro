#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "VulkanDevice.h"

#include <vector>

namespace Hydro
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	// Presentation only: the swapchain images, their views and framebuffers, the render pass that
	// draws into them, and acquire/present. Per-frame command buffers and fences live in
	// VulkanFrameContext; the surface lives in VulkanRendererContext.
	class VulkanSwapChain
	{
	public:
		// `device` must outlive the swapchain (both belong to Renderer, which shuts this down first).
		VulkanSwapChain(VulkanDevice& device, VkSurfaceKHR surface);

		// Returns false when the surface is currently 0x0 (minimised); call Recreate later.
		bool Create(uint32_t width, uint32_t height, bool vsync);
		// Rebuilds only what depends on the swapchain images. The caller must make sure the GPU is idle.
		bool Recreate(uint32_t width, uint32_t height);
		void Shutdown();

		void SetVSync(bool vsync) { m_VSync = vsync; }
		bool IsVSync() const { return m_VSync; }

		VkResult AcquireNextImage(VkSemaphore imageAvailable, uint32_t& outImageIndex);
		VkResult Present(uint32_t imageIndex);

		void BeginRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void EndRenderPass(VkCommandBuffer commandBuffer);

		bool IsValid() const { return m_SwapChain != VK_NULL_HANDLE; }

		VkRenderPass GetRenderPass() const { return m_RenderPass; }
		VkExtent2D GetExtent() const { return m_Extent; }
		VkFormat GetImageFormat() const { return m_SurfaceFormat.format; }
		uint32_t GetImageCount() const { return (uint32_t)m_Images.size(); }
		uint32_t GetMinImageCount() const { return m_MinImageCount; }

		// One per swapchain image: the presentation engine may still hold an image's semaphore
		// while other frames are being recorded, so these must not be shared per frame in flight.
		VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const { return m_RenderFinishedSemaphores[imageIndex]; }

	private:
		bool CreateSwapChainResources(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapChain);
		void DestroySwapChainResources();
		void CreateRenderPass();

		SwapChainSupportDetails QuerySwapChainSupport() const;
		static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync);
		static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

	private:
		VulkanDevice* m_Device = nullptr; // not owned
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		VkSurfaceFormatKHR m_SurfaceFormat{};
		VkExtent2D m_Extent{ 0, 0 };
		uint32_t m_MinImageCount = 0;
		bool m_VSync = true;

		// Created once; only depends on the surface format.
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;

		// Rebuilt on every Recreate.
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;
		std::vector<VkFramebuffer> m_Framebuffers;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
	};
}
