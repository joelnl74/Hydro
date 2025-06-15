#include "hypch.h"
#include "VulkanRenderCommandBuffer.h"
#include "../../Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(uint32_t count, const std::string& debugName)
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice();

		if (count == 0)
		{
			// 0 means one per frame in flight
			count = 2;
		}

		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = device->GetPhysicalDevice()->GetQueueFamilyIndices().graphics;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		vkCreateCommandPool(device->GetDevice(), &cmdPoolInfo, nullptr, &m_CommandPool);

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = m_CommandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = count;
		m_CommandBuffers.resize(count);
		
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device->GetDevice(), &commandBufferAllocateInfo, m_CommandBuffers.data()));

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		m_WaitFences.resize(count);
		for (size_t i = 0; i < m_WaitFences.size(); ++i)
		{
			VK_CHECK_RESULT(vkCreateFence(device->GetDevice(), &fenceCreateInfo, nullptr, &m_WaitFences[i]));
		}
	}

	VulkanRenderCommandBuffer::VulkanRenderCommandBuffer(const std::string& debugName)
	{
	}

	void VulkanRenderCommandBuffer::Begin()
	{
		uint32_t commandBufferIndex = Renderer::GetRenderFrame() % m_CommandBuffers.size();

		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		cmdBufInfo.pNext = nullptr;

		VkCommandBuffer commandBuffer = nullptr;
		if (m_OwnedBySwapChain)
		{
			// VulkanSwapChain& swapChain = Application::Get().GetWindow().GetSwapChain();
			// commandBuffer = swapChain.GetDrawCommandBuffer(commandBufferIndex);
		}
		else
		{
			commandBufferIndex %= m_CommandBuffers.size();
			commandBuffer = m_CommandBuffers[commandBufferIndex];
		}
		
		m_ActiveCommandBuffer = commandBuffer;
		vkResetCommandBuffer(m_ActiveCommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
		VK_CHECK_RESULT(vkBeginCommandBuffer(m_ActiveCommandBuffer, &cmdBufInfo));
	}

	void VulkanRenderCommandBuffer::End()
	{
		uint32_t commandBufferIndex = Renderer::GetRenderFrame() % m_CommandBuffers.size();
		if (m_OwnedBySwapChain)
			commandBufferIndex %= m_CommandBuffers.size();

		VkCommandBuffer commandBuffer = m_ActiveCommandBuffer;
		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		m_ActiveCommandBuffer = nullptr;
	}

	void VulkanRenderCommandBuffer::Submit()
	{
		auto device = Renderer::GetRendererContext()->GetVulkanDevice();

		uint32_t commandBufferIndex = Renderer::GetRenderFrame() % m_CommandBuffers.size();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = m_CommandBuffers[commandBufferIndex];
		submitInfo.pCommandBuffers = &commandBuffer;

		VK_CHECK_RESULT(vkWaitForFences(device->GetDevice(), 1, &m_WaitFences[commandBufferIndex], VK_TRUE, UINT64_MAX));
		VK_CHECK_RESULT(vkResetFences(device->GetDevice(), 1, &m_WaitFences[commandBufferIndex]));

		HY_CORE_TRACE("Renderer", "Submitting Render Command Buffer {}", m_DebugName);

		VK_CHECK_RESULT(vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFences[commandBufferIndex]));
	}
}
