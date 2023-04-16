#include "hypch.h"
#include "VulkanCommandBuffer.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	VkCommandBuffer VulkanCommandBuffer::Create()
	{
		auto context = Renderer::GetRendererContext();
		auto device = context->GetVulkanDevice()->GetDevice();
		auto commandPool = Renderer::GetVulkanSwapChain()->GetCommandPool();

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		vkAllocateCommandBuffers(device, &allocInfo, &m_commandBuffer);

		return m_commandBuffer;
	}

	void VulkanCommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
	}

	void VulkanCommandBuffer::End()
	{
		auto context = Renderer::GetRendererContext();
		auto& device = context->GetVulkanDevice();
		auto commandPool = Renderer::GetVulkanSwapChain()->GetCommandPool();

		vkEndCommandBuffer(m_commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_commandBuffer;

		vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device->GetGraphicsQueue());

		vkFreeCommandBuffers(device->GetDevice(), commandPool, 1, &m_commandBuffer);
	}

}

