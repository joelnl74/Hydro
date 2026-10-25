#pragma once
#include "Hydro/Core/Base.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <filesystem>
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

		// Saves the next picture this view renders as a PNG at `path` (folders are created). The image
		// is copied to host memory right after the view's pass and written when that frame's GPU work
		// has finished (a frame or two later), so nothing waits for the GPU; the PNG is encoded on the
		// main thread, a short hitch for a large view. Alpha is written opaque. The file is logged when
		// written. A view that isn't rendered (hidden window, minimised) keeps the request until it is.
		// A second request before the first is recorded replaces it.
		void RequestCapture(const std::filesystem::path& path);
		// True from RequestCapture until its file is written (or failed).
		bool IsCapturePending() const;

		const RenderViewSpecification& GetSpecification() const { return m_Specification; }

	private:
		RenderViewSpecification m_Specification;
		Scope<RenderViewData> m_Data;

		friend class Renderer2D;
	};
}
