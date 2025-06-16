#include "hypch.h"
#include "VulkanRenderPass.h"
#include "../../Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanRenderPass::VulkanRenderPass()
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

		if (vkCreateRenderPass(device->GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}

		CreateImage();
		CreateFrameBuffer();
	}

	void VulkanRenderPass::CreateImage()
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();
		auto& size = Renderer::GetVulkanSwapChain()->GetExtend();

		for (size_t i = 0; i <MAX_FRAMES_IN_FLIGHT; i++)
		{
			ImageSpecification spec
			{
			};

			spec.Width = size.width;
			spec.Height = size.height;

			m_image.push_back(VulkanImage(spec));
		}

		/*
		for (size_t i = 0; i < m_image.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_image[i].GetVulkanImageInfo().Image;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VK_CHECK_RESULT(vkCreateImageView(device->GetDevice(), &createInfo, nullptr, &m_imageViews[i]));
		}
		*/
	}

	void VulkanRenderPass::CreateFrameBuffer()
	{
		auto& size = Renderer::GetVulkanSwapChain()->GetExtend();
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();

		m_Framebuffer.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkImageView attachments[] = {
				m_image[i].GetVulkanImageInfo().ImageView
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = size.width;
			framebufferInfo.height = size.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}
}

