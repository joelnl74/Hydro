#pragma once
#include "Hydro/Core/Window.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanShader.h"

namespace Hydro
{
	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	const int MAX_FRAMES_IN_FLIGHT = 2;

	class VulkanPresentation
	{
	public:
		VulkanPresentation(Window& window) : m_window(window) {}
		void Init(VkInstance instance, Ref<VulkanDevice> vulkanDevice);
		void InitSurface(Window &window);
		void CreateSwapChain(Window& window ,bool vsync);
		void CreatePresentationLayer();
		void CreateRenderPass();
		void CreateFrameBuffer();
		void CreateCommandPool();
		void CreateCommandBuffer();
		void CreateSyncObjects();

		VkCommandBuffer& GetCommandBuffer() { return m_CommandBuffers[m_CurrentFrame]; }
		VkExtent2D& GetExtend() { return m_SwapChainExtent; }
		VkRenderPass& GetRenderPass() { return m_RenderPass; }
		std::vector<VkPipelineShaderStageCreateInfo> & GetShaderInfo() { return ShaderStages;  }

		void ResetSwapChain();

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

		VkRenderPass m_RenderPass;
		VkCommandPool m_CommandPool;
		std::vector <VkCommandBuffer> m_CommandBuffers;

		std::vector <VkSemaphore> m_ImageSemaphores;
		std::vector <VkSemaphore> m_RenderSemaphores;
		std::vector <VkFence> m_Fences;

		uint32_t m_CurrentFrame = 0;
		uint32_t imageIndex;

		Window& m_window;

		// TODO REMOVE
		Ref<VulkanShader> m_VertShader;
		Ref<VulkanShader> m_FragmentShader;
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
	};
}

