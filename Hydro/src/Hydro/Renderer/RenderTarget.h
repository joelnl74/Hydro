#pragma once
#include "Hydro/Renderer/Image.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The images a view (or an effect such as the 2D light buffer) renders into. Made by
	// VulkanRenderTarget; with dynamic rendering there are no VkRenderPass or VkFramebuffer objects,
	// so pipelines are built from these formats (PipelineSpecification::ColorFormats / DepthFormat).
	struct RenderTargetSpecification
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		// One image per entry, in attachment order (location 0, 1, ... in the fragment shader).
		std::vector<ImageFormat> ColorFormats = { ImageFormat::SRGB };
		// ImageFormat::None: no depth attachment.
		ImageFormat DepthFormat = ImageFormat::None;
		// An extra R32I colour attachment after ColorFormats, for click-to-select (Editor view only).
		bool EntityID = false;
		std::string DebugName = "RenderTarget";
	};

	// What happens to an attachment's contents when rendering begins.
	enum class AttachmentLoadOp : uint8_t
	{
		Clear = 0, // filled with the clear value
		Load,      // kept from what was rendered into it before (this frame or earlier)
		DontCare   // undefined: everything will be drawn over
	};

	// Load ops and clear values for one RenderTarget Begin. Every attachment is listed explicitly.
	struct RenderTargetBeginInfo
	{
		AttachmentLoadOp ColorLoad = AttachmentLoadOp::Clear;   // every colour attachment except the entity ID
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };    // linear RGBA
		AttachmentLoadOp DepthLoad = AttachmentLoadOp::Clear;
		float ClearDepth = 1.0f;
		AttachmentLoadOp EntityIDLoad = AttachmentLoadOp::Clear;
		int32_t ClearEntityID = -1;

		// Clear every attachment, the colours to `color`.
		static RenderTargetBeginInfo Clear(const glm::vec4& color)
		{
			RenderTargetBeginInfo info;
			info.ClearColor = color;
			return info;
		}
	};
}
