#pragma once
#include "Hydro/Core/Window.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanPipeline.h"

namespace Hydro
{
	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	class VulkanPresentation
	{
	public:
		void Init(VkInstance instance, Ref<VulkanDevice> vulkanDevice);
		void InitSurface(Window &window);
		void CreateSwapChain(Window& window ,bool vsync);
		void CreatePresentationLayer();
		void CreateRenderPass();
		void CreateFrameBuffer();
		void CreateCommandPool();
		void CreateCommandBuffer();
		void CreateSyncObjects();


		void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void DrawFrame();
		void BeginRenderPass();
		void EndRenderPass();

		void ShutDown();

	private:
		SwapChainSupportDetails QuerySwapChainSupport();
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window);

		void CreateImageViews();

	private:
		inline static VkSurfaceKHR s_vkSurface;
		VkInstance m_instance;
		Ref<VulkanDevice> m_Device;

		VkSwapchainKHR m_SwapChain;
		VkQueue m_PresentQueue;

		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;

		SwapChainSupportDetails m_SwapChainSupportDetails;

		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		std::vector<VkFramebuffer> swapChainFramebuffers;

		VkRenderPass m_RenderPass;
		VkCommandPool m_CommandPool;
		VkCommandBuffer m_CommandBuffer;

		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_RenderFinishedSemaphore;
		VkFence m_InFlightFence;

		// TODO REMOVE
		VulkanPipeline m_VulkanPipeline;
		Ref<VulkanShader> m_VertShader;
		Ref<VulkanShader> m_FragmentShader;
		VkPipelineShaderStageCreateInfo ShaderStages[2];
	};
}

