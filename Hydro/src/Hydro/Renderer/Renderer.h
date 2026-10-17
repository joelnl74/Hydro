#pragma once
#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanSwapChain.h"
#include "Hydro/Platform/Vulkan/VulkanFrameContext.h"

#include "Hydro/Core/Window.h"
#include "Hydro/Renderer/RendererSettings.h"

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

		static void Init(Window& window);
		static void Shutdown();

		static void RenderFrame();
		static void OnWindowResize(uint32_t width, uint32_t height);
		static void WaitIdle();

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

		static Ref<VulkanRendererContext> GetRendererContext() { return s_Instance->m_RendererContext; }
		static Ref<VulkanSwapChain> GetVulkanSwapChain() { return s_Instance->m_SwapChain; }

	private:
		bool BeginFrame(uint32_t& outImageIndex);
		void EndFrame(uint32_t imageIndex);
		bool RecreateSwapChain();

	private:
		static Renderer* s_Instance;
		static RendererSettings s_Settings;

		Ref<VulkanRendererContext> m_RendererContext;
		Ref<VulkanSwapChain> m_SwapChain;
		VulkanFrameContext m_Frames;

		PresentPassCallback m_PresentPassCallback;
		RendererCapabilities m_Capabilities;
		char m_GPUName[256] = {};

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_SwapChainDirty = false;
	};
}
