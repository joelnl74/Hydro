#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/RenderFeatures.h"

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Hydro
{
	struct RenderViewSpecification
	{
		uint32_t Width = 1280;
		uint32_t Height = 720;
		std::string DebugName = "View";
		// The editor's scene view: gets the editor-only render features (entity ID, debug lines).
		bool EditorView = false;
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
	// Its backend data (RenderViewData, Platform/Vulkan/VulkanRenderViewData.h) is filled and recorded by
	// Renderer2D; what is drawn each frame is the view's render graph, built from its render features.
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
		ImTextureID GetImGuiTexture();

		// The render features the next render uses (SceneRenderer sets them every frame from the scene's
		// override or the project's RendererSettings::Features). The view's render graph is rebuilt when
		// they change, never otherwise. Starts as the 2D preset.
		void SetRenderFeatures(const RenderFeatureSettings& features);
		const RenderFeatureSettings& GetRenderFeatures() const;
		// What the view's current render graph reported when it was built (empty before the first render).
		const std::vector<RenderValidationMessage>& GetValidationMessages() const;

		// Saves the next picture this view renders as a PNG at `path` (folders are created). The image
		// is copied to host memory right after the view's pass and written when that frame's GPU work
		// has finished (a frame or two later), so nothing waits for the GPU; the PNG is encoded on the
		// main thread, a short hitch for a large view. Alpha is written opaque. The file is logged when
		// written. A view that isn't rendered (hidden window, minimised) keeps the request until it is.
		// A second request before the first is recorded replaces it.
		void RequestCapture(const std::filesystem::path& path);
		// True from RequestCapture until its file is written (or failed).
		bool IsCapturePending() const;

		// Click-to-select (M7): copies the entity ID under pixel (x, y) of the view's image (top-left origin,
		// in the view's own pixels) after its next render; the answer arrives a frame or two later through
		// TakeEntityIDResult, without waiting for the GPU. Only views with the entity-ID attachment (the
		// Editor view) answer. A newer request replaces an older one; an answer is dropped when the view
		// was resized or InvalidateEntityIDRequests was called since its request.
		void RequestEntityIDReadback(uint32_t x, uint32_t y);
		// True once per answer: `entityID` is the entt entity drawn there, or -1 for nothing.
		bool TakeEntityIDResult(int32_t& entityID);
		// The scene changed (opened, closed, Play, Stop): answers to earlier clicks are thrown away.
		void InvalidateEntityIDRequests();

		const RenderViewSpecification& GetSpecification() const { return m_Specification; }

	private:
		RenderViewSpecification m_Specification;
		Scope<RenderViewData> m_Data;

		friend class Renderer2D;
		friend class Renderer3D;
	};
}
