#pragma once
#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanSwapChain.h"
#include "Hydro/Platform/Vulkan/VulkanFrameContext.h"

#include "Hydro/Core/Window.h"
#include "Hydro/Renderer/RendererSettings.h"

#include <deque>
#include <functional>

namespace Hydro
{
	// Renderer subsystem. Owns the GPU context, the swapchain and the per-frame resources, and runs
	// one frame: acquire -> record (scene passes, then the present pass) -> submit -> present.
	//
	// Explicit lifetime: Application calls Init after the window and Shutdown before it. Init starts
	// the renderer's parts in order (context, swapchain, frames, allocator, Renderer2D, Renderer3D)
	// and Shutdown stops them in reverse.
	class Renderer
	{
	public:
		using PresentPassCallback = std::function<void(VkCommandBuffer)>;
		using ResourceFreeFunction = std::function<void()>;

		static void Init(Window& window);
		static void Shutdown();

		static void RenderFrame();
		static void OnWindowResize(uint32_t width, uint32_t height);
		static void WaitIdle();

		// Deletion queue. Runs `freeFunction` once the GPU has finished every frame that may still use
		// what it frees: frames already submitted and the one being recorded. That is at the start of
		// the frame MAX_FRAMES_IN_FLIGHT frames later (after its fence wait), after a swapchain rebuild
		// (the device is idle then) or at Renderer::Shutdown, whichever comes first.
		// Destructors of GPU resources hand their handles to it, so a resource can be released at any
		// point in the frame without Renderer::WaitIdle. The function must only capture handles, never
		// the object being destroyed.
		static void SubmitResourceFree(ResourceFreeFunction freeFunction);

		// Live options (clear colour, vsync, wireframe, shader hot reload). May be called before Init;
		// after Init, changes apply from the next frame (a vsync change recreates the swapchain).
		static const RendererSettings& GetSettings() { return s_Settings; }
		static void SetSettings(const RendererSettings& settings);

		// What the GPU supports. Valid after Init.
		static const RendererCapabilities& GetCapabilities() { return s_Instance->m_Capabilities; }

		// Called inside the swapchain render pass every frame (the ImGui layer draws the UI here).
		static void SetPresentPassCallback(PresentPassCallback callback) { s_Instance->m_PresentPassCallback = std::move(callback); }

		// Index of the frame in flight being recorded: use it to pick per-frame buffers and descriptor sets.
		static uint32_t GetRenderFrame() { return s_Instance->m_Frames.GetFrameIndex(); }
		static VkCommandBuffer GetCurrentCommandBuffer() { return s_Instance->m_Frames.GetCommandBuffer(); }

		// Backend access (Platform/Vulkan, Renderer2D, ImGuiLayer). Valid between Init and Shutdown.
		// The device is the common case (handles, queues, SubmitImmediate); the context is only for the
		// instance and surface.
		static VulkanDevice& GetVulkanDevice() { return s_Instance->m_RendererContext->GetVulkanDevice(); }
		static VulkanRendererContext& GetRendererContext() { return *s_Instance->m_RendererContext; }
		static VulkanSwapChain& GetVulkanSwapChain() { return *s_Instance->m_SwapChain; }

	private:
		bool BeginFrame(uint32_t& outImageIndex);
		void EndFrame(uint32_t imageIndex);
		bool RecreateSwapChain();

		// Runs the queued frees whose frames the GPU has finished (all of them when `all` is true;
		// only call that when the device is idle).
		void FlushResourceFrees(bool all);

	private:
		struct PendingResourceFree
		{
			uint64_t Frame = 0; // the last frame that may use the resource
			ResourceFreeFunction Free;
		};

	private:
		static Renderer* s_Instance;
		static RendererSettings s_Settings;

		Scope<VulkanRendererContext> m_RendererContext;
		Scope<VulkanSwapChain> m_SwapChain;
		VulkanFrameContext m_Frames;

		// Frames submitted so far, which is also the number of the frame being (or about to be) recorded.
		uint64_t m_FrameNumber = 0;
		std::deque<PendingResourceFree> m_ResourceFreeQueue; // in submission order, so sorted by Frame

		PresentPassCallback m_PresentPassCallback;
		RendererCapabilities m_Capabilities;
		char m_GPUName[256] = {};

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_SwapChainDirty = false;
	};
}
