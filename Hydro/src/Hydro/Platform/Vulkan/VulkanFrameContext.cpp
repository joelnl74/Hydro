#include "hypch.h"
#include "VulkanFrameContext.h"

#include "VulkanUtils.h"

namespace Hydro
{
	void VulkanFrameContext::Init(Ref<VulkanDevice> device)
	{
		m_Device = device;
		m_FrameIndex = 0;

		VkDevice vkDevice = m_Device->GetDevice();
		uint32_t graphicsFamily = (uint32_t)m_Device->GetPhysicalDevice()->GetQueueFamilyIndices().graphics;

		for (FrameData& frame : m_Frames)
		{
			// One pool per frame, so a whole frame's command memory can be reset at once later on.
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = graphicsFamily;
			VK_CHECK_RESULT(vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &frame.CommandPool));

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = frame.CommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(vkDevice, &allocInfo, &frame.CommandBuffer));

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // First WaitForFrame must not block.
			VK_CHECK_RESULT(vkCreateFence(vkDevice, &fenceInfo, nullptr, &frame.InFlightFence));

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &frame.ImageAvailable));
		}
	}

	void VulkanFrameContext::ShutDown()
	{
		if (m_Device == nullptr)
			return;

		VkDevice vkDevice = m_Device->GetDevice();

		for (FrameData& frame : m_Frames)
		{
			vkDestroySemaphore(vkDevice, frame.ImageAvailable, nullptr);
			vkDestroyFence(vkDevice, frame.InFlightFence, nullptr);
			vkDestroyCommandPool(vkDevice, frame.CommandPool, nullptr); // Frees its command buffer too.
			frame = FrameData{};
		}

		m_Device = nullptr;
	}

	void VulkanFrameContext::WaitForFrame()
	{
		VK_CHECK_RESULT(vkWaitForFences(m_Device->GetDevice(), 1, &m_Frames[m_FrameIndex].InFlightFence, VK_TRUE, UINT64_MAX));
	}

	void VulkanFrameContext::ResetFence()
	{
		VK_CHECK_RESULT(vkResetFences(m_Device->GetDevice(), 1, &m_Frames[m_FrameIndex].InFlightFence));
	}

	VkCommandBuffer VulkanFrameContext::BeginCommandBuffer()
	{
		VkCommandBuffer commandBuffer = m_Frames[m_FrameIndex].CommandBuffer;
		VK_CHECK_RESULT(vkResetCommandBuffer(commandBuffer, 0));

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		return commandBuffer;
	}

	void VulkanFrameContext::EndCommandBuffer()
	{
		VK_CHECK_RESULT(vkEndCommandBuffer(m_Frames[m_FrameIndex].CommandBuffer));
	}

	void VulkanFrameContext::Submit(VkQueue queue, VkSemaphore renderFinished)
	{
		FrameData& frame = m_Frames[m_FrameIndex];

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &frame.ImageAvailable;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame.CommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinished;

		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, frame.InFlightFence));
	}
}
