#include "hypch.h"
#include "VulkanRenderPass.h"
#include "../../Renderer/Renderer.h"
#include "VulkanUtils.h"

namespace Hydro
{
	VulkanRenderPass::VulkanRenderPass(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		CreateRenderPass();
		CreateTargets();
	}

	void VulkanRenderPass::CreateRenderPass()
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

		// The image is written here and sampled by the ImGui pass (fragment shader).
		// [0]: wait for earlier fragment-shader reads of this image before writing it again.
		// [1]: make our writes visible to the ImGui pass that samples it afterwards.
		VkSubpassDependency dependencies[2]{};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies;

		if (vkCreateRenderPass(device->GetDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void VulkanRenderPass::CreateTargets()
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();

		m_images.clear();
		m_Framebuffers.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			ImageSpecification spec{};
			spec.Width = m_Width;
			spec.Height = m_Height;
			m_images.push_back(VulkanImage(spec));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkImageView attachments[] = {
				m_images[i].GetVulkanImageInfo().ImageView
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_Width;
			framebufferInfo.height = m_Height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void VulkanRenderPass::DestroyTargets()
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();

		for (VkFramebuffer framebuffer : m_Framebuffers)
		{
			if (framebuffer)
				vkDestroyFramebuffer(device->GetDevice(), framebuffer, nullptr);
		}
		m_Framebuffers.clear();

		for (VulkanImage& image : m_images)
			image.Destroy();
		m_images.clear();
	}

	void VulkanRenderPass::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || (width == m_Width && height == m_Height))
			return;

		DestroyTargets();
		m_Width = width;
		m_Height = height;
		CreateTargets();
	}

	void VulkanRenderPass::Destroy()
	{
		auto& device = Renderer::GetRendererContext()->GetVulkanDevice();

		DestroyTargets();
		if (m_renderPass)
			vkDestroyRenderPass(device->GetDevice(), m_renderPass, nullptr);
		m_renderPass = VK_NULL_HANDLE;
	}
}
