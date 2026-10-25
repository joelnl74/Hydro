#pragma once
#include <vulkan/vulkan.h>

#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/RenderTarget.h"
#include "VulkanFrameContext.h"
#include "VulkanImage.h"

#include <array>
#include <vector>

namespace Hydro
{
	// Colour, optional depth and optional entity-ID images rendered with dynamic rendering
	// (vkCmdBeginRendering), one set per frame in flight, so a frame never writes images that an
	// earlier, still-running frame is reading.
	//
	// A resource: made from a RenderTargetSpecification, non-copyable, held in a Scope. Its images free
	// themselves through Renderer::SubmitResourceFree, so it can be resized or destroyed mid-frame.
	//
	// Layouts are tracked per image. Begin moves every attachment to its attachment layout; End ends
	// rendering and leaves the colour attachments (entity ID included) in SHADER_READ_ONLY_OPTIMAL,
	// which is what everything that reads them expects (ImGui, the light composite, captures), and the
	// depth attachment in DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
	class VulkanRenderTarget
	{
	public:
		explicit VulkanRenderTarget(const RenderTargetSpecification& specification);
		~VulkanRenderTarget() = default;

		VulkanRenderTarget(const VulkanRenderTarget&) = delete;
		VulkanRenderTarget& operator=(const VulkanRenderTarget&) = delete;

		// Makes new images of the new size; the old ones go through the deletion queue (frames in flight
		// may still use them). Anything that refers to them (an ImGui texture) must be pointed at the new
		// ones. Does nothing when the size is unchanged; 0 is not allowed (callers skip minimised views).
		void Resize(uint32_t width, uint32_t height);

		// Transitions `frame`'s attachments and begins rendering into all of them.
		void Begin(VkCommandBuffer commandBuffer, uint32_t frame, const RenderTargetBeginInfo& info);
		// Ends rendering; colour attachments go to SHADER_READ_ONLY_OPTIMAL (see the class comment).
		void End(VkCommandBuffer commandBuffer, uint32_t frame);

		// Colour attachment 0 of `frame` (what views show and capture).
		const VulkanImage& GetImage(uint32_t frame) const { return *m_Frames[frame].Colors[0]; }
		const VulkanImage& GetColorImage(uint32_t frame, uint32_t index) const { return *m_Frames[frame].Colors[index]; }
		// Null when the target has no depth / entity-ID attachment.
		const VulkanImage* GetDepthImage(uint32_t frame) const { return m_Frames[frame].Depth.get(); }
		const VulkanImage* GetEntityIDImage(uint32_t frame) const { return m_Frames[frame].EntityID.get(); }

		const RenderTargetSpecification& GetSpecification() const { return m_Specification; }
		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		VkExtent2D GetExtent() const { return { m_Specification.Width, m_Specification.Height }; }

		// The attachment layout pipelines are built for: colour formats in order (the entity ID last when
		// there is one) and the depth format. Two targets with equal layouts share pipelines.
		std::vector<ImageFormat> GetPipelineColorFormats() const;
		ImageFormat GetPipelineDepthFormat() const { return m_Specification.DepthFormat; }

	private:
		struct FrameImages
		{
			std::vector<Scope<VulkanImage>> Colors;
			std::vector<VkImageLayout> ColorLayouts;
			Scope<VulkanImage> Depth;
			VkImageLayout DepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			Scope<VulkanImage> EntityID;
			VkImageLayout EntityIDLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		};

		void CreateImages();

	private:
		RenderTargetSpecification m_Specification;
		std::array<FrameImages, MAX_FRAMES_IN_FLIGHT> m_Frames;
	};
}
