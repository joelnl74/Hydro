#include <hypch.h>

#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Core/Time.h"

namespace Hydro
{
	Renderer* Renderer::s_Instance = nullptr;

#ifdef HY_DIST
	bool Renderer::s_ShaderHotReload = false;
#else
	bool Renderer::s_ShaderHotReload = true;
#endif

	void Renderer::Init(Window& window)
	{
		HY_CORE_ASSERT(s_Instance == nullptr, "Renderer::Init called twice");

		s_Instance = new Renderer();
		Renderer& renderer = *s_Instance;

		renderer.m_Width = window.GetWidth();
		renderer.m_Height = window.GetHeight();

		// 1. Instance, surface, device.
		renderer.m_RendererContext = CreateRef<VulkanRendererContext>();
		renderer.m_RendererContext->Init(window);

		Ref<VulkanDevice> device = renderer.m_RendererContext->GetVulkanDevice();

		// 2. Swapchain.
		renderer.m_SwapChain = CreateRef<VulkanSwapChain>(device, renderer.m_RendererContext->GetSurface());
		if (renderer.m_SwapChain->Create(renderer.m_Width, renderer.m_Height, true) == false)
		{
			// Started minimised: build it on the first frame the window has a size.
			renderer.m_SwapChainDirty = true;
		}

		// 3. Per-frame command buffers and sync objects.
		renderer.m_Frames.Init(device);

		// 4. GPU memory allocator.
		VulkanAllocator::Init(device);

		// Offscreen targets need a non-zero size even if the window starts minimised.
		VkExtent2D extent = renderer.m_SwapChain->GetExtent();
		uint32_t targetWidth = extent.width > 0 ? extent.width : std::max(renderer.m_Width, 1u);
		uint32_t targetHeight = extent.height > 0 ? extent.height : std::max(renderer.m_Height, 1u);

		// 5. Sub-renderers.
		Renderer2D::Init(targetWidth, targetHeight);
		Renderer3D::Init();
	}

	void Renderer::Shutdown()
	{
		if (s_Instance == nullptr)
			return;

		WaitIdle();

		// Exactly the reverse of Init.
		Renderer3D::Shutdown();
		Renderer2D::Shutdown();
		VulkanAllocator::Shutdown();

		s_Instance->m_Frames.Shutdown();
		s_Instance->m_SwapChain->Shutdown();
		s_Instance->m_SwapChain = nullptr;
		s_Instance->m_RendererContext->Shutdown();
		s_Instance->m_RendererContext = nullptr;

		delete s_Instance;
		s_Instance = nullptr;
	}

	void Renderer::WaitIdle()
	{
		if (s_Instance && s_Instance->m_RendererContext && s_Instance->m_RendererContext->GetVulkanDevice())
			s_Instance->m_RendererContext->GetVulkanDevice()->WaitIdle();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		s_Instance->m_Width = width;
		s_Instance->m_Height = height;
		s_Instance->m_SwapChainDirty = true;
	}

	void Renderer::RenderFrame()
	{
		Renderer* renderer = s_Instance;

		// Render = fence wait + acquire + recording; Present = submit + present (includes vsync).
		Time::BeginPhase(FramePhase::Render);

		uint32_t imageIndex = 0;
		if (renderer->BeginFrame(imageIndex) == false)
		{
			// Nothing will be drawn this frame; drop what was queued so it does not pile up.
			Renderer2D::DiscardFrame();
			Time::EndPhase(FramePhase::Render);
			return;
		}

		VkCommandBuffer commandBuffer = renderer->m_Frames.BeginCommandBuffer();

		// Offscreen scene passes first. Their render passes make the results readable by fragment
		// shaders, so the present pass below can sample them (the editor viewport does).
		Renderer2D::Render(commandBuffer);

		renderer->m_SwapChain->BeginRenderPass(commandBuffer, imageIndex);
		if (renderer->m_PresentPassCallback)
			renderer->m_PresentPassCallback(commandBuffer);
		renderer->m_SwapChain->EndRenderPass(commandBuffer);

		renderer->m_Frames.EndCommandBuffer();
		Time::EndPhase(FramePhase::Render);

		Time::BeginPhase(FramePhase::Present);
		renderer->EndFrame(imageIndex);
		Time::EndPhase(FramePhase::Present);
	}

	bool Renderer::BeginFrame(uint32_t& outImageIndex)
	{
		if (m_SwapChainDirty)
		{
			if (RecreateSwapChain() == false)
				return false;
		}

		m_Frames.WaitForFrame();

		VkResult result = m_SwapChain->AcquireNextImage(m_Frames.GetImageAvailableSemaphore(), outImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_SwapChainDirty = true;
			return false;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			HY_CORE_ERROR("vkAcquireNextImageKHR failed: {0}", VKResultToString(result));
			return false;
		}

		// Reset only now: if we bailed out above, the fence must stay signalled or the next wait deadlocks.
		m_Frames.ResetFence();
		return true;
	}

	void Renderer::EndFrame(uint32_t imageIndex)
	{
		Ref<VulkanDevice> device = m_RendererContext->GetVulkanDevice();

		m_Frames.Submit(device->GetGraphicsQueue(), m_SwapChain->GetRenderFinishedSemaphore(imageIndex));

		VkResult result = m_SwapChain->Present(imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			m_SwapChainDirty = true;
		}
		else if (result != VK_SUCCESS)
		{
			HY_CORE_ERROR("vkQueuePresentKHR failed: {0}", VKResultToString(result));
		}

		m_Frames.Advance();
	}

	bool Renderer::RecreateSwapChain()
	{
		if (m_Width == 0 || m_Height == 0)
			return false; // Minimised.

		m_RendererContext->GetVulkanDevice()->WaitIdle();

		// Recreate also covers the case where the swapchain was never created (started minimised).
		if (m_SwapChain->Recreate(m_Width, m_Height) == false)
			return false;

		// Renderer2D's offscreen target is sized by the editor viewport, not the window, so it is
		// not touched here.
		m_SwapChainDirty = false;
		return true;
	}
}
