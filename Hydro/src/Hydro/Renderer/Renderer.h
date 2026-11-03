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
	// Owns the GPU context, swapchain and per-frame resources, and runs a frame: acquire, record, submit, present.
	// Application calls Init after the window and Shutdown before it.
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

		// Deletion queue: runs `freeFunction` once the GPU has finished every frame that may still use what it frees, so a
		// resource can be released at any point in a frame. Capture only handles, never the object being destroyed.
		static void SubmitResourceFree(ResourceFreeFunction freeFunction);

		// May be called before Init; after it, changes apply from the next frame (vsync recreates the swapchain).
		static const RendererSettings& GetSettings() { return s_Settings; }
		static void SetSettings(const RendererSettings& settings);

		// What the GPU supports. Valid after Init.
		static const RendererCapabilities& GetCapabilities() { return s_Instance->m_Capabilities; }

		// Called inside the swapchain render pass every frame (the ImGui layer draws the UI here).
		static void SetPresentPassCallback(PresentPassCallback callback) { s_Instance->m_PresentPassCallback = std::move(callback); }

		// Index of the frame in flight being recorded: use it to pick per-frame buffers and descriptor sets.
		static uint32_t GetRenderFrame() { return s_Instance->m_Frames.GetFrameIndex(); }
		static VkCommandBuffer GetCurrentCommandBuffer() { return s_Instance->m_Frames.GetCommandBuffer(); }

		// Backend access, valid between Init and Shutdown. The context is only for the instance and surface.
		static VulkanDevice& GetVulkanDevice() { return s_Instance->m_RendererContext->GetVulkanDevice(); }
		static VulkanRendererContext& GetRendererContext() { return *s_Instance->m_RendererContext; }
		static VulkanSwapChain& GetVulkanSwapChain() { return *s_Instance->m_SwapChain; }

	private:
		bool BeginFrame(uint32_t& outImageIndex);
		void EndFrame(uint32_t imageIndex);
		bool RecreateSwapChain();

		// Runs the queued frees whose frames the GPU has finished (all of them when `all`: only when the device is idle).
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
