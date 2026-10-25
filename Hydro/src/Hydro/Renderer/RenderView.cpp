#include "hypch.h"
#include "RenderView.h"

#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/Renderer2D.h"
#include "Hydro/Platform/Vulkan/VulkanRenderViewData.h"

#include <Hydro/Platform/Vulkan/imgui_impl_vulkan.h>

namespace Hydro
{
	RenderView::RenderView(const RenderViewSpecification& specification)
		: m_Specification(specification), m_Data(CreateScope<RenderViewData>())
	{
		const uint32_t width = specification.Width > 0 ? specification.Width : 1;
		const uint32_t height = specification.Height > 0 ? specification.Height : 1;

		RenderViewData& view = *m_Data;
		view.DebugName = specification.DebugName;
		view.EditorView = specification.EditorView;
		view.Features = RenderFeatureSettings::Preset2D();

		RenderTargetSpecification targetSpecification;
		targetSpecification.Width = width;
		targetSpecification.Height = height;
		targetSpecification.ColorFormats = { ViewColorFormat };
		targetSpecification.DebugName = specification.DebugName;
		view.Target = CreateScope<VulkanRenderTarget>(targetSpecification);
		view.RequestedWidth = width;
		view.RequestedHeight = height;

		view.UniformBuffer = CreateScope<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));

		// Batches, texture sets and the view list belong to Renderer2D.
		Renderer2D::RegisterView(view);
	}

	RenderView::~RenderView()
	{
		RenderViewData& view = *m_Data;

		// Never leave a dangling pointer in Renderer2D's view list or this frame's queue.
		Renderer2D::UnregisterView(view);

		for (QuadVertex* base : view.QuadVertexBufferBases)
		{
			delete[] base;
		}

		// The GPU objects (vertex and uniform buffers, descriptor sets, target) free themselves when
		// m_Data goes, through the deletion queue, so frames in flight can still finish with them.
		// The ImGui texture handles belong to ImGui's descriptor pool, which the ImGui layer frees
		// (this ImGui version has no RemoveTexture).
	}

	void RenderView::SetSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		m_Data->RequestedWidth = width;
		m_Data->RequestedHeight = height;
	}

	uint32_t RenderView::GetWidth() const
	{
		return m_Data->RequestedWidth;
	}

	uint32_t RenderView::GetHeight() const
	{
		return m_Data->RequestedHeight;
	}

	ImTextureID RenderView::GetImGuiTexture()
	{
		const uint32_t frame = Renderer::GetRenderFrame();
		VkDescriptorSet& set = m_Data->ImGuiTextures[frame];

		if (set == VK_NULL_HANDLE)
		{
			const VulkanImageInfo& image = m_Data->Target->GetImage(frame).GetVulkanImageInfo();
			set = ImGui_ImplVulkan_AddTexture(image.Sampler, image.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		return (ImTextureID)set;
	}

	void RenderView::SetRenderFeatures(const RenderFeatureSettings& features)
	{
		m_Data->Features = features;
	}

	const RenderFeatureSettings& RenderView::GetRenderFeatures() const
	{
		return m_Data->Features;
	}

	const std::vector<RenderValidationMessage>& RenderView::GetValidationMessages() const
	{
		static const std::vector<RenderValidationMessage> none;
		return m_Data->Graph != nullptr ? m_Data->Graph->GetValidationMessages() : none;
	}

	void RenderView::RequestCapture(const std::filesystem::path& path)
	{
		m_Data->RequestedCapture = path;
	}

	bool RenderView::IsCapturePending() const
	{
		if (m_Data->RequestedCapture.empty() == false)
		{
			return true;
		}

		for (const std::filesystem::path& path : m_Data->CapturePaths)
		{
			if (path.empty() == false)
			{
				return true;
			}
		}

		return false;
	}
}
