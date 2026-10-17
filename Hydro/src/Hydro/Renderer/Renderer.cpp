#include <hypch.h>

#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"

#include <chrono>

namespace Hydro
{
	Renderer* Renderer::s_Instance = nullptr;

#ifdef HY_DIST
	bool Renderer::s_ShaderHotReload = false;
#else
	bool Renderer::s_ShaderHotReload = true;
#endif

	void Renderer::Create(Window& window)
	{
		s_Instance = new Renderer();
		s_Instance->Init(window);
	}

	void Renderer::Init(Window& window)
	{
		m_Width = window.GetWidth();
		m_Height = window.GetHeight();

		m_RendererContext = CreateRef<VulkanRendererContext>();
		m_RendererContext->Init(window);

		Ref<VulkanDevice> device = m_RendererContext->GetVulkanDevice();

		m_SwapChain = CreateRef<VulkanSwapChain>(device, m_RendererContext->GetSurface());
		if (m_SwapChain->Create(m_Width, m_Height, true) == false)
		{
			// Started minimised: build it on the first frame the window has a size.
			m_SwapChainDirty = true;
		}

		m_Frames.Init(device);

		VulkanAllocator::Init(device);

		// Offscreen targets need a non-zero size even if the window starts minimised.
		VkExtent2D extent = m_SwapChain->GetExtent();
		uint32_t targetWidth = extent.width > 0 ? extent.width : std::max(m_Width, 1u);
		uint32_t targetHeight = extent.height > 0 ? extent.height : std::max(m_Height, 1u);

		Renderer2D::Init(targetWidth, targetHeight);
		Renderer3D::Init();
	}

	void Renderer::ShutDown()
	{
		if (s_Instance == nullptr)
			return;

		WaitIdle();

		Renderer2D::ShutDown();
		Renderer3D::ShutDown();
		VulkanAllocator::Shutdown();

		s_Instance->m_Frames.ShutDown();
		s_Instance->m_SwapChain->ShutDown();
		s_Instance->m_SwapChain = nullptr;
		s_Instance->m_RendererContext->ShutDown();

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
		using Clock = std::chrono::steady_clock;
		auto Ms = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<float, std::milli>(b - a).count(); };

		Renderer* renderer = s_Instance;
		const Clock::time_point frameStart = Clock::now();
		renderer->m_LastRecordMs = 0.0f;
		renderer->m_LastPresentMs = 0.0f;

		uint32_t imageIndex = 0;
		if (renderer->BeginFrame(imageIndex) == false)
		{
			// Nothing will be drawn this frame; drop what was queued so it does not pile up.
			Renderer2D::DiscardFrame();
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
		const Clock::time_point afterRecord = Clock::now();

		renderer->EndFrame(imageIndex);

		renderer->m_LastRecordMs = Ms(frameStart, afterRecord);
		renderer->m_LastPresentMs = Ms(afterRecord, Clock::now());
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
