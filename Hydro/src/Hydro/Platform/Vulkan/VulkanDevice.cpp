#include "hypch.h"

#include "VulkanDevice.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

#include <set>

namespace Hydro
{
	VulkanDevice::VulkanDevice(Scope<VulkanPhysicalDevice> physicalDevice)
		: m_PhysicalDevice(std::move(physicalDevice))
	{
		const QueueFamilyIndices& indices = m_PhysicalDevice->GetQueueFamilyIndices();

		// One queue per unique family: graphics and present are often, but not always, the same family.
		std::set<uint32_t> uniqueFamilies = { (uint32_t)indices.graphics, (uint32_t)indices.present };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		float queuePriority = 1.0f;
		for (uint32_t family : uniqueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = family;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Only enable features the GPU actually has.
		VkPhysicalDeviceFeatures deviceFeatures{};
		m_SamplerAnisotropyEnabled = m_PhysicalDevice->GetFeatures().samplerAnisotropy == VK_TRUE;
		deviceFeatures.samplerAnisotropy = m_SamplerAnisotropyEnabled ? VK_TRUE : VK_FALSE;
		// Needed by PipelineSpecification::Wireframe and ::LineWidth != 1.
		m_FillModeNonSolidEnabled = m_PhysicalDevice->GetFeatures().fillModeNonSolid == VK_TRUE;
		deviceFeatures.fillModeNonSolid = m_FillModeNonSolidEnabled ? VK_TRUE : VK_FALSE;
		m_WideLinesEnabled = m_PhysicalDevice->GetFeatures().wideLines == VK_TRUE;
		deviceFeatures.wideLines = m_WideLinesEnabled ? VK_TRUE : VK_FALSE;

		std::vector<const char*> deviceExtensions = VulkanPhysicalDevice::GetRequiredExtensions();

		// A portability driver (MoltenVK or KosmicKrisp on macOS) reports VK_KHR_portability_subset, and
		// the spec requires enabling it when it is offered. The name is spelled out because the macro is
		// in vulkan_beta.h.
		if (m_PhysicalDevice->SupportsExtension("VK_KHR_portability_subset"))
		{
			deviceExtensions.push_back("VK_KHR_portability_subset");
			HY_CORE_INFO("Portability driver: VK_KHR_portability_subset enabled");
		}

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		VkResult result = vkCreateDevice(m_PhysicalDevice->GetVulkanPhysicalDevice(), &deviceCreateInfo, nullptr, &m_LogicalDevice);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Creating logical device failed.");
		}

		vkGetDeviceQueue(m_LogicalDevice, (uint32_t)indices.graphics, 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_LogicalDevice, (uint32_t)indices.present, 0, &m_PresentQueue);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = (uint32_t)indices.graphics;

		VK_CHECK_RESULT(vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_UploadCommandPool));
	}

	void VulkanDevice::SubmitImmediate(const std::function<void(VkCommandBuffer)>& record)
	{
		HY_CORE_ASSERT(m_LogicalDevice != VK_NULL_HANDLE, "VulkanDevice::SubmitImmediate after Shutdown");

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_UploadCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		record(commandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		// Wait on a fence for this submit only, not the whole queue: frames in flight keep running.
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateFence(m_LogicalDevice, &fenceInfo, nullptr, &fence));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence));
		VK_CHECK_RESULT(vkWaitForFences(m_LogicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(m_LogicalDevice, fence, nullptr);
		vkFreeCommandBuffers(m_LogicalDevice, m_UploadCommandPool, 1, &commandBuffer);
	}

	void VulkanDevice::WaitIdle() const
	{
		if (m_LogicalDevice != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(m_LogicalDevice);
		}
	}

	void VulkanDevice::Shutdown()
	{
		if (m_LogicalDevice == VK_NULL_HANDLE)
		{
			return;
		}

		vkDestroyCommandPool(m_LogicalDevice, m_UploadCommandPool, nullptr);
		m_UploadCommandPool = VK_NULL_HANDLE;

		vkDestroyDevice(m_LogicalDevice, nullptr);
		m_LogicalDevice = VK_NULL_HANDLE;
	}
}
