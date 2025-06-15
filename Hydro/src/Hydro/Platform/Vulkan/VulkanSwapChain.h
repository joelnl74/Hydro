#pragma once
#include "Hydro/Core/Window.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"

namespace Hydro
{
	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	const int MAX_FRAMES_IN_FLIGHT = 2;

	class VulkanSwapChain
	{
	public:
		VulkanSwapChain(Window& window) : m_window(window) {}
		void Init(VkInstance instance, Ref<VulkanDevice> vulkanDevice);
		void InitSurface(Window &window);
		void CreateSwapChain(Window& window ,bool vsync);
		void CreateRenderPass();
		void CreateFrameBuffer();
		void CreateCommandPool();
		void CreateCommandBuffer();
		void CreateSyncObjects();
		void CreateImageSampler();


		VkCommandBuffer& GetCommandBuffer() { return m_CommandBuffers[m_CurrentFrame]; }
		VkCommandPool& GetCommandPool() { return m_CommandPool; }

		VkExtent2D& GetExtend() { return m_SwapChainExtent; }
		VkRenderPass& GetRenderPass() { return m_RenderPass; }

		uint32_t GetRenderFrame() { return m_CurrentFrame; }
		uint32_t GetRenderImage() { return imageIndex; }

		void CreateVulkanComposeImage();

		void ResetSwapChain();
		void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void DrawFrame();

		void StartShutDown();
		void ShutDown();

	private:
		SwapChainSupportDetails QuerySwapChainSupport();
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window);

		void CreateImageViews();

	private:
		inline static VkSurfaceKHR s_Surface;
		VkInstance m_Instance;
		Ref<VulkanDevice> m_Device;

		VkSwapchainKHR m_SwapChain;
		VkQueue m_PresentQueue;

		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;

		SwapChainSupportDetails m_SwapChainSupportDetails;

		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		std::vector<VkFramebuffer> m_SwapChainFramebuffers;
		VkSampler m_SwapChainSampler;	

		VkRenderPass m_RenderPass;
		VkCommandPool m_CommandPool;
		std::vector <VkCommandBuffer> m_CommandBuffers;
		std::vector <VkDescriptorSet> m_DescriptorSets;

		std::vector <VkSemaphore> m_ImageSemaphores;
		std::vector <VkSemaphore> m_RenderSemaphores;
		std::vector <VkFence> m_Fences;

		uint32_t m_CurrentFrame = 0;
		uint32_t imageIndex = 0;

		Window& m_window;
	};
}

