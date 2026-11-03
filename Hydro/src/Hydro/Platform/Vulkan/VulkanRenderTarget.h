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
	// Colour, optional depth and entity-ID images for dynamic rendering, one set per frame in flight. End leaves colour
	// in SHADER_READ_ONLY_OPTIMAL and depth in DEPTH_STENCIL_ATTACHMENT_OPTIMAL (shader-read with SampledDepth).
	class VulkanRenderTarget
	{
	public:
		explicit VulkanRenderTarget(const RenderTargetSpecification& specification);
		~VulkanRenderTarget() = default;

		VulkanRenderTarget(const VulkanRenderTarget&) = delete;
		VulkanRenderTarget& operator=(const VulkanRenderTarget&) = delete;

		// New images of the new size; the old ones go through the deletion queue, so whatever refers to them (an ImGui
		// texture) must be updated. No-op when unchanged; 0 is not allowed.
		void Resize(uint32_t width, uint32_t height);

		// `colorTarget` (the HDR scene colour): draw into its colour images with this target's depth and entity ID;
		// it must be the same size.
		void Begin(VkCommandBuffer commandBuffer, uint32_t frame, const RenderTargetBeginInfo& info,
			VulkanRenderTarget* colorTarget = nullptr);
		// Colour attachments (`colorTarget`'s when given to Begin) go to SHADER_READ_ONLY_OPTIMAL.
		void End(VkCommandBuffer commandBuffer, uint32_t frame, VulkanRenderTarget* colorTarget = nullptr);

		// Colour attachment 0 (what views show and capture).
		const VulkanImage& GetImage(uint32_t frame) const { return *m_Frames[frame].Colors[0]; }
		const VulkanImage& GetColorImage(uint32_t frame, uint32_t index) const { return *m_Frames[frame].Colors[index]; }
		// Null when the target has no depth / entity-ID attachment.
		const VulkanImage* GetDepthImage(uint32_t frame) const { return m_Frames[frame].Depth.get(); }
		const VulkanImage* GetEntityIDImage(uint32_t frame) const { return m_Frames[frame].EntityID.get(); }

		const RenderTargetSpecification& GetSpecification() const { return m_Specification; }
		uint32_t GetWidth() const { return m_Specification.Width; }
		uint32_t GetHeight() const { return m_Specification.Height; }
		VkExtent2D GetExtent() const { return { m_Specification.Width, m_Specification.Height }; }

		// Colour formats in order (entity ID last) for building pipelines; equal layouts share pipelines. With
		// `colorTarget`: the layout of a pass begun with it.
		std::vector<ImageFormat> GetPipelineColorFormats(const VulkanRenderTarget* colorTarget = nullptr) const;
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
