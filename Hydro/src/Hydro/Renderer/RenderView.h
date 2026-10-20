#pragma once
#include "Hydro/Core/Base.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace Hydro
{
	struct RenderViewSpecification
	{
		uint32_t Width = 1280;
		uint32_t Height = 720;
		std::string DebugName = "View";
	};

	struct RenderViewData;

	// One picture of a scene: an offscreen image, the camera it is seen through, and the quads
	// queued for it this frame. Renderer2D::BeginScene(view, camera) ... DrawQuad ... EndScene()
	// fills it, and Renderer2D::Render records one 2D pass per view that was begun this frame.
	// The editor has two (Editor and Game windows); each is shown with ImGui::Image.
	//
	// A resource: create it after Renderer::Init and destroy it before Renderer::Shutdown (layers do
	// this in OnDetach). Its GPU objects are freed through the deletion queue, so it can be destroyed
	// at any point in a frame.
	// Its contents belong to Renderer2D, which implements it (Renderer2D.cpp).
	class RenderView
	{
	public:
		explicit RenderView(const RenderViewSpecification& specification);
		~RenderView();

		RenderView(const RenderView&) = delete;
		RenderView& operator=(const RenderView&) = delete;

		// The size to render at, applied at the start of the next Renderer2D::Render. 0 is ignored.
		void SetSize(uint32_t width, uint32_t height);
		// The requested size (what the next render uses).
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		float GetAspectRatio() const { return (float)GetWidth() / (float)GetHeight(); }

		// ImGui texture of the image of the frame being recorded: pass it to ImGui::Image.
		VkDescriptorSet GetImGuiTexture();

		const RenderViewSpecification& GetSpecification() const { return m_Specification; }

	private:
		RenderViewSpecification m_Specification;
		Scope<RenderViewData> m_Data;

		friend class Renderer2D;
	};
}
