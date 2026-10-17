#include "hypch.h"
#include "VulkanRenderPass.h"
#include "Hydro/Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanRenderPass::VulkanRenderPass(uint32_t width, uint32_t height, ImageFormat format)
		: m_Format(format), m_Width(width), m_Height(height)
	{
		CreateRenderPass();
		CreateTargets();
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
		DestroyTargets();

		if (m_renderPass != VK_NULL_HANDLE)
		{
			VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();
			vkDestroyRenderPass(device, m_renderPass, nullptr);
		}
	}

	void VulkanRenderPass::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || (width == m_Width && height == m_Height))
			return;

		m_Width = width;
		m_Height = height;

		DestroyTargets();
		CreateTargets();
	}

	void VulkanRenderPass::CreateRenderPass()
	{
		VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = VulkanImage::GetImageFormat(m_Format);
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

		VkSubpassDependency dependencies[2]{};

		// Before: earlier fragment-shader reads of this image must finish before we overwrite it.
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// After: our colour writes must be visible to fragment shaders that sample the image
		// later in the same submission (the ImGui viewport in the present pass).
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass));
	}

	void VulkanRenderPass::CreateTargets()
	{
		VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		m_Images.resize(MAX_FRAMES_IN_FLIGHT);
		m_Framebuffers.resize(MAX_FRAMES_IN_FLIGHT);

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			ImageSpecification spec{};
			spec.Width = m_Width;
			spec.Height = m_Height;
			spec.Format = m_Format;
			spec.Usage = ImageUsage::Attachment;

			m_Images[i] = CreateScope<VulkanImage>(spec);

			VkImageView attachment = m_Images[i]->GetVulkanImageInfo().ImageView;

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &attachment;
			framebufferInfo.width = m_Width;
			framebufferInfo.height = m_Height;
			framebufferInfo.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]));
		}
	}

	void VulkanRenderPass::DestroyTargets()
	{
		VkDevice device = Renderer::GetRendererContext()->GetVulkanDevice()->GetDevice();

		for (VkFramebuffer framebuffer : m_Framebuffers)
			vkDestroyFramebuffer(device, framebuffer, nullptr);

		m_Framebuffers.clear();
		m_Images.clear(); // VulkanImage destructors free the images, views and samplers.
	}

	void VulkanRenderPass::Begin(VkCommandBuffer commandBuffer, uint32_t frameIndex, const glm::vec4& color)
	{
		VkClearValue clearColor{};
		clearColor.color = { { color.r, color.g, color.b, color.a } };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = m_Framebuffers[frameIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_Width, m_Height };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanRenderPass::End(VkCommandBuffer commandBuffer)
	{
		vkCmdEndRenderPass(commandBuffer);
	}
}
