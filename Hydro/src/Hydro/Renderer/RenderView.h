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

	// One picture of a scene: an offscreen image, its camera and the quads queued for it this frame. Renderer2D fills
	// and records it through its render graph. Create after Renderer::Init, destroy before Renderer::Shutdown.
	class RenderView
	{
	public:
		explicit RenderView(const RenderViewSpecification& specification);
		~RenderView();

		RenderView(const RenderView&) = delete;
		RenderView& operator=(const RenderView&) = delete;

		// The size to render at, applied at the start of the view's next render. 0 is ignored.
		void SetSize(uint32_t width, uint32_t height);
		// The requested size (what the next render uses).
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		float GetAspectRatio() const { return (float)GetWidth() / (float)GetHeight(); }

		// ImGui texture of the image of the frame being recorded: pass it to ImGui::Image.
		ImTextureID GetImGuiTexture();

		// The render features the next render uses (SceneRenderer sets them every frame). The render graph is rebuilt
		// only when they change. Starts as the 2D preset.
		void SetRenderFeatures(const RenderFeatureSettings& features);
		const RenderFeatureSettings& GetRenderFeatures() const;
		// What the view's current render graph reported when it was built (empty before the first render).
		const std::vector<RenderValidationMessage>& GetValidationMessages() const;

		// Saves the next picture this view renders as a PNG, without waiting for the GPU (written a frame or two later).
		// A view that isn't rendered keeps the request; a newer request replaces it.
		void RequestCapture(const std::filesystem::path& path);
		// True from RequestCapture until its file is written (or failed).
		bool IsCapturePending() const;

		// Reads back the entity ID under pixel (x, y) (top-left origin) after the next render; TakeEntityIDResult gets it a
		// frame or two later. Only the Editor view answers; a resize or InvalidateEntityIDRequests drops the request.
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
