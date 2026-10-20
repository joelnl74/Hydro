#include "hypch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	// One-time-submit command buffer for uploads and layout transitions. End() submits and waits.
	VkCommandBuffer VulkanCommandBuffer::Create()
	{
		VulkanDevice& device = Renderer::GetRendererContext().GetVulkanDevice();

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = device.GetUploadCommandPool();
		allocInfo.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(device.GetDevice(), &allocInfo, &m_commandBuffer));

		return m_commandBuffer;
	}

	void VulkanCommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));
	}

	void VulkanCommandBuffer::End()
	{
		VulkanDevice& device = Renderer::GetRendererContext().GetVulkanDevice();

		VK_CHECK_RESULT(vkEndCommandBuffer(m_commandBuffer));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_commandBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(device.GetGraphicsQueue()));

		vkFreeCommandBuffers(device.GetDevice(), device.GetUploadCommandPool(), 1, &m_commandBuffer);
		m_commandBuffer = VK_NULL_HANDLE;
	}
}
