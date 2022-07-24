#include "hypch.h"
#include "VulkanPresentation.h"
#include <set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "VulkanUtils.h"

namespace Hydro
{
	void VulkanPresentation::Init(VkInstance instance, Ref<VulkanDevice> vulkanDevice)
	{
		m_Device = vulkanDevice;
		m_instance = instance;
	}
	void VulkanPresentation::InitSurface(Window& window)
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		auto glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(glfwWindow);
		createInfo.hinstance = GetModuleHandle(nullptr);

		if (vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &s_vkSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		if (glfwCreateWindowSurface(m_instance, glfwWindow, nullptr, &s_vkSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		// Get available queue family properties
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportsPresent(queueCount);

		uint32_t presentFamily = 0;

		for (uint32_t i = 0; i < queueCount; i++)
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, s_vkSurface, &supportsPresent[i]);

			if (supportsPresent[i]) 
			{
				presentFamily = i;
				break;
			}
		}

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { (uint32_t)m_Device->GetPhysicalDevice()->GetQueueFamilyIndices().graphics, presentFamily};

		vkGetDeviceQueue(m_Device->GetDevice(), presentFamily, 0, &m_PresentQueue);
	}
	void VulkanPresentation::CreateSwapChain(Window& window, bool vsync)
	{
		VkDevice device = m_Device->GetDevice();
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();

		m_SwapChainSupportDetails = QuerySwapChainSupport();

		bool swapChainAdequate = false;
		swapChainAdequate = !m_SwapChainSupportDetails.formats.empty() && !m_SwapChainSupportDetails.presentModes.empty();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(m_SwapChainSupportDetails.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(m_SwapChainSupportDetails.presentModes, vsync);
		VkExtent2D extent = ChooseSwapExtent(m_SwapChainSupportDetails.capabilities, window);

		uint32_t imageCount = m_SwapChainSupportDetails.capabilities.minImageCount + 1;

		if (m_SwapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > m_SwapChainSupportDetails.capabilities.maxImageCount)
		{
			imageCount = m_SwapChainSupportDetails.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = s_vkSurface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// TODO Check this with the queue family.
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		createInfo.preTransform = m_SwapChainSupportDetails.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;


		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;

		CreateImageViews();
		CreateRenderPass();
		CreatePresentationLayer();
		m_VulkanPipeline.Create(m_SwapChainExtent, m_RenderPass, ShaderStages);
		CreateFrameBuffer();
		CreateCommandPool();
		CreateCommandBuffer();
		CreateSyncObjects();
	}

	void VulkanPresentation::CreatePresentationLayer()
	{
		m_FragmentShader = CreateRef<VulkanShader>();
		m_VertShader = CreateRef<VulkanShader>();

		auto vertexShader = m_VertShader->Create("vertex.spv");
		auto fragmentShader = m_FragmentShader->Create("fragment.spv");

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

		vertShaderStageInfo.module = vertexShader;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragmentShader;
		fragShaderStageInfo.pName = "main";

		ShaderStages[0] = vertShaderStageInfo;
		ShaderStages[1] = fragShaderStageInfo;
	}

	void VulkanPresentation::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_SwapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(m_Device->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void VulkanPresentation::CreateFrameBuffer()
	{
		swapChainFramebuffers.resize(m_SwapChainImageViews.size());

		for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) {
			VkImageView attachments[] = {
				m_SwapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_SwapChainExtent.width;
			framebufferInfo.height = m_SwapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(m_Device->GetDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}

	}

	void VulkanPresentation::CreateCommandPool()
	{
		auto physicalDevice = m_Device->GetPhysicalDevice();

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = physicalDevice->GetQueueFamilyIndices().graphics;

		if (vkCreateCommandPool(m_Device->GetDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void VulkanPresentation::CreateCommandBuffer()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, &m_CommandBuffer) != VK_SUCCESS) 
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void VulkanPresentation::CreateSyncObjects()
	{
		auto device = m_Device->GetDevice();

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}

	void VulkanPresentation::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_SwapChainExtent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 1.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VulkanPipeline.GetPipeLine());
		vkCmdSetViewport(commandBuffer, 0, 1, &m_VulkanPipeline.GetViewPort());
		vkCmdSetScissor(commandBuffer, 0, 1, &m_VulkanPipeline.GetRect2D());
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void VulkanPresentation::DrawFrame()
	{
		auto device = m_Device->GetDevice();

		vkWaitForFences(device, 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &m_InFlightFence);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		vkResetCommandBuffer(m_CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
		RecordCommandBuffer(m_CommandBuffer, imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_CommandBuffer;

		VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { m_SwapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	}

	void VulkanPresentation::BeginRenderPass()
	{

	}

	void VulkanPresentation::EndRenderPass()
	{
	}

	void VulkanPresentation::ShutDown()
	{
		auto device = m_Device->GetDevice();

		vkDeviceWaitIdle(device);

		vkDestroySemaphore(device, m_RenderFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, m_ImageAvailableSemaphore, nullptr);
		vkDestroyFence(device, m_InFlightFence, nullptr);

		vkDestroyCommandPool(device, m_CommandPool, nullptr);

		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		m_VulkanPipeline.ShutDown();
		vkDestroyRenderPass(device, m_RenderPass, nullptr);

		for (auto imageView : m_SwapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, m_SwapChain, nullptr);

		vkDestroySurfaceKHR(m_instance, s_vkSurface, nullptr);
	}

	SwapChainSupportDetails VulkanPresentation::QuerySwapChainSupport()
	{
		VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice()->GetVulkanPhysicalDevice();
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, s_vkSurface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, s_vkSurface, &formatCount, nullptr);

		if (formatCount != 0) 
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, s_vkSurface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, s_vkSurface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, s_vkSurface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}
	VkSurfaceFormatKHR VulkanPresentation::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR VulkanPresentation::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync)
	{
		if (vsync)
		{
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanPresentation::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window)
	{
		auto glfwWindow = static_cast<GLFWwindow*>(window.GetNativeWindow());

		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
		{
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(glfwWindow, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void VulkanPresentation::CreateImageViews()
	{
		auto device = m_Device->GetDevice();

		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i = 0; i < m_SwapChainImages.size(); i++) 
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_SwapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_SwapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &m_SwapChainImageViews[i]));
		}
	}
}
