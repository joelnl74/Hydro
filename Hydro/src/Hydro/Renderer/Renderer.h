#pragma once
#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanSwapChain.h"
#include "Hydro/Platform/Vulkan/VulkanFrameContext.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

#include "Hydro/Core/Window.h"

#include <functional>

namespace Hydro
{
	// Owns the GPU context, the swapchain and the per-frame resources, and runs one frame:
	// acquire -> record (scene passes, then the present pass) -> submit -> present.
	class Renderer
	{
	public:
		using PresentPassCallback = std::function<void(VkCommandBuffer)>;

		static void Create(Window& window);
		static void ShutDown();

		static void RenderFrame();
		static void OnWindowResize(uint32_t width, uint32_t height);
		static void WaitIdle();

		// Rebuild shaders when their source files change (checked a few times per second).
		// On by default, except in Dist builds. Can be set before Create().
		static void SetShaderHotReload(bool enabled) { s_ShaderHotReload = enabled; }
		static bool IsShaderHotReloadEnabled() { return s_ShaderHotReload; }

		// Called inside the swapchain render pass every frame (the ImGui layer draws the UI here).
		static void SetPresentPassCallback(PresentPassCallback callback) { s_Instance->m_PresentPassCallback = std::move(callback); }

		// Index of the frame in flight being recorded: use it to pick per-frame buffers and descriptor sets.
		static uint32_t GetRenderFrame() { return s_Instance->m_Frames.GetFrameIndex(); }
		static VkCommandBuffer GetCurrentCommandBuffer() { return s_Instance->m_Frames.GetCommandBuffer(); }

		// CPU time of the last RenderFrame: fence wait + acquire + recording, and submit + present.
		static float GetLastRecordMs() { return s_Instance->m_LastRecordMs; }
		static float GetLastPresentMs() { return s_Instance->m_LastPresentMs; }

		static Ref<VulkanRendererContext> GetRendererContext() { return s_Instance->m_RendererContext; }
		static Ref<VulkanSwapChain> GetVulkanSwapChain() { return s_Instance->m_SwapChain; }

	private:
		void Init(Window& window);
		bool BeginFrame(uint32_t& outImageIndex);
		void EndFrame(uint32_t imageIndex);
		bool RecreateSwapChain();

	private:
		static Renderer* s_Instance;
		static bool s_ShaderHotReload;

		Ref<VulkanRendererContext> m_RendererContext;
		Ref<VulkanSwapChain> m_SwapChain;
		VulkanFrameContext m_Frames;

		PresentPassCallback m_PresentPassCallback;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_SwapChainDirty = false;

		float m_LastRecordMs = 0.0f;
		float m_LastPresentMs = 0.0f;
	};
}
