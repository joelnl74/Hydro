#pragma once
#include "Hydro/Renderer/Image.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The images a view or effect renders into. With dynamic rendering there are no render passes, so pipelines are
	// built from these formats.
	struct RenderTargetSpecification
	{
		uint32_t Width = 1;
		uint32_t Height = 1;
		// One image per entry, in attachment order. May be empty for a depth-only target, which then needs a DepthFormat.
		std::vector<ImageFormat> ColorFormats = { ImageFormat::SRGB };
		// ImageFormat::None: no depth attachment.
		ImageFormat DepthFormat = ImageFormat::None;
		// The depth is sampled after End (a shadow map, the depth of field): End moves it to SHADER_READ_ONLY_OPTIMAL.
		bool SampledDepth = false;
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
