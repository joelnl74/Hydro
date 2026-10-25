#include "hypch.h"
#include "VulkanRenderTarget.h"

namespace Hydro
{
	static VkAttachmentLoadOp ToVulkanLoadOp(AttachmentLoadOp loadOp)
	{
		switch (loadOp)
		{
			case AttachmentLoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
			case AttachmentLoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			default:                         return VK_ATTACHMENT_LOAD_OP_CLEAR;
		}
	}

	// A layout transition of one whole image (one mip, one layer). The stages and accesses cover every
	// way the targets are used: drawn into, sampled by fragment shaders (ImGui, the composite) and
	// copied (captures, ID readback).
	static VkImageMemoryBarrier2 MakeTransition(VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to)
	{
		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.oldLayout = from;
		barrier.newLayout = to;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = { aspect, 0, 1, 0, 1 };

		const VkPipelineStageFlags2 readers = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		const VkPipelineStageFlags2 depthTests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

		switch (from)
		{
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				barrier.srcStageMask = depthTests;
				barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Reads only: an execution dependency is enough (no writes to make available).
				barrier.srcStageMask = readers;
				barrier.srcAccessMask = 0;
				break;
			default: // UNDEFINED: nothing to wait for
				barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
				barrier.srcAccessMask = 0;
				break;
		}

		switch (to)
		{
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				barrier.dstStageMask = depthTests;
				barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;
			default: // SHADER_READ_ONLY_OPTIMAL
				barrier.dstStageMask = readers;
				barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT;
				break;
		}

		return barrier;
	}

	static void SubmitBarriers(VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier2>& barriers)
	{
		if (barriers.empty())
		{
			return;
		}

		VkDependencyInfo dependency{};
		dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency.imageMemoryBarrierCount = (uint32_t)barriers.size();
		dependency.pImageMemoryBarriers = barriers.data();
		vkCmdPipelineBarrier2(commandBuffer, &dependency);
	}

	// Depth images use DEPTH_STENCIL_ATTACHMENT_OPTIMAL (valid for depth-only formats too):
	// DEPTH_ATTACHMENT_OPTIMAL would need the separateDepthStencilLayouts feature.
	static VkImageAspectFlags GetDepthAspect(ImageFormat format)
	{
		return format == ImageFormat::DEPTH24STENCIL8 ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VulkanRenderTarget::VulkanRenderTarget(const RenderTargetSpecification& specification)
		: m_Specification(specification)
	{
		HY_CORE_ASSERT(specification.ColorFormats.empty() == false, "RenderTargetSpecification needs at least one colour format");
		HY_CORE_ASSERT(specification.DepthFormat == ImageFormat::None || VulkanImage::IsDepthFormat(specification.DepthFormat),
			"RenderTargetSpecification::DepthFormat must be a depth format or None");

		m_Specification.Width = std::max(specification.Width, 1u);
		m_Specification.Height = std::max(specification.Height, 1u);
		CreateImages();
	}

	void VulkanRenderTarget::CreateImages()
	{
		for (FrameImages& frame : m_Frames)
		{
			// Dropping the old Scopes queues their frees (VulkanImage's destructor).
			frame = FrameImages{};

			ImageSpecification image{};
			image.Width = m_Specification.Width;
			image.Height = m_Specification.Height;
			image.Usage = ImageUsage::Attachment;

			for (ImageFormat format : m_Specification.ColorFormats)
			{
				image.Format = format;
				frame.Colors.push_back(CreateScope<VulkanImage>(image));
				frame.ColorLayouts.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
			}

			if (m_Specification.DepthFormat != ImageFormat::None)
			{
				image.Format = m_Specification.DepthFormat;
				frame.Depth = CreateScope<VulkanImage>(image);
			}

			if (m_Specification.EntityID)
			{
				image.Format = ImageFormat::R32I;
				frame.EntityID = CreateScope<VulkanImage>(image);
			}
		}
	}

	void VulkanRenderTarget::Resize(uint32_t width, uint32_t height)
	{
		HY_CORE_ASSERT(width > 0 && height > 0, "VulkanRenderTarget::Resize to 0");
		if (width == m_Specification.Width && height == m_Specification.Height)
		{
			return;
		}

		m_Specification.Width = width;
		m_Specification.Height = height;
		CreateImages();
	}

	std::vector<ImageFormat> VulkanRenderTarget::GetPipelineColorFormats() const
	{
		std::vector<ImageFormat> formats = m_Specification.ColorFormats;
		if (m_Specification.EntityID)
		{
			formats.push_back(ImageFormat::R32I);
		}
		return formats;
	}

	void VulkanRenderTarget::Begin(VkCommandBuffer commandBuffer, uint32_t frameIndex, const RenderTargetBeginInfo& info)
	{
		HY_CORE_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT, "VulkanRenderTarget::Begin: frame index out of range");
		FrameImages& frame = m_Frames[frameIndex];

		std::vector<VkImageMemoryBarrier2> barriers;
		std::vector<VkRenderingAttachmentInfo> colorAttachments;

		const auto addColor = [&](const VulkanImage& image, VkImageLayout& layout, AttachmentLoadOp loadOp, const VkClearValue& clear)
		{
			barriers.push_back(MakeTransition(image.GetVulkanImageInfo().Image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkRenderingAttachmentInfo attachment{};
			attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			attachment.imageView = image.GetVulkanImageInfo().ImageView;
			attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachment.loadOp = ToVulkanLoadOp(loadOp);
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.clearValue = clear;
			colorAttachments.push_back(attachment);
		};

		VkClearValue clearColor{};
		clearColor.color = { { info.ClearColor.r, info.ClearColor.g, info.ClearColor.b, info.ClearColor.a } };
		for (size_t i = 0; i < frame.Colors.size(); i++)
		{
			addColor(*frame.Colors[i], frame.ColorLayouts[i], info.ColorLoad, clearColor);
		}

		if (frame.EntityID != nullptr)
		{
			VkClearValue clearID{};
			clearID.color.int32[0] = info.ClearEntityID;
			addColor(*frame.EntityID, frame.EntityIDLayout, info.EntityIDLoad, clearID);
		}

		VkRenderingAttachmentInfo depthAttachment{};
		if (frame.Depth != nullptr)
		{
			barriers.push_back(MakeTransition(frame.Depth->GetVulkanImageInfo().Image, GetDepthAspect(m_Specification.DepthFormat),
				frame.DepthLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
			frame.DepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView = frame.Depth->GetVulkanImageInfo().ImageView;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = ToVulkanLoadOp(info.DepthLoad);
			// Kept: a later section of the same frame may load it (cut-out sprites after meshes).
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.clearValue.depthStencil = { info.ClearDepth, 0 };
		}

		SubmitBarriers(commandBuffer, barriers);

		VkRenderingInfo rendering{};
		rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		rendering.renderArea = { { 0, 0 }, GetExtent() };
		rendering.layerCount = 1;
		rendering.colorAttachmentCount = (uint32_t)colorAttachments.size();
		rendering.pColorAttachments = colorAttachments.data();
		rendering.pDepthAttachment = frame.Depth != nullptr ? &depthAttachment : nullptr;
		vkCmdBeginRendering(commandBuffer, &rendering);
	}

	void VulkanRenderTarget::End(VkCommandBuffer commandBuffer, uint32_t frameIndex)
	{
		HY_CORE_ASSERT(frameIndex < MAX_FRAMES_IN_FLIGHT, "VulkanRenderTarget::End: frame index out of range");
		vkCmdEndRendering(commandBuffer);

		// Colour attachments are read next (ImGui, the composite, captures); depth stays where it is.
		FrameImages& frame = m_Frames[frameIndex];
		std::vector<VkImageMemoryBarrier2> barriers;
		for (size_t i = 0; i < frame.Colors.size(); i++)
		{
			barriers.push_back(MakeTransition(frame.Colors[i]->GetVulkanImageInfo().Image, VK_IMAGE_ASPECT_COLOR_BIT,
				frame.ColorLayouts[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			frame.ColorLayouts[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		if (frame.EntityID != nullptr)
		{
			barriers.push_back(MakeTransition(frame.EntityID->GetVulkanImageInfo().Image, VK_IMAGE_ASPECT_COLOR_BIT,
				frame.EntityIDLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			frame.EntityIDLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		SubmitBarriers(commandBuffer, barriers);
	}
}
