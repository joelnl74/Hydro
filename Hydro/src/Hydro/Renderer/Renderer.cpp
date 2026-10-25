#include <hypch.h>

#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"
#include "Hydro/Platform/Vulkan/VulkanDescriptors.h"
#include "Hydro/Core/Time.h"

namespace Hydro
{
	Renderer* Renderer::s_Instance = nullptr;

	RendererSettings Renderer::s_Settings;

	// One line per group at startup, so a log from another machine says what its GPU can do.
	static void LogCapabilities(const RendererCapabilities& capabilities)
	{
		const auto yesNo = [](bool value) { return value ? "yes" : "no"; };
		HY_CORE_INFO("Renderer: {0}, Vulkan {1}.{2}.{3}, {4} frames in flight", capabilities.GPUName,
			capabilities.VulkanVersionMajor, capabilities.VulkanVersionMinor, capabilities.VulkanVersionPatch,
			capabilities.FramesInFlight);
		HY_CORE_INFO("Renderer: required: dynamic rendering {0}, synchronization2 {1}",
			yesNo(capabilities.DynamicRendering), yesNo(capabilities.Synchronization2));
		HY_CORE_INFO("Renderer: optional: wireframe {0}, wide lines {1}, independent blend {2}, sampler anisotropy {3} (max {4}), descriptor indexing {5} (not enabled)",
			yesNo(capabilities.Wireframe), yesNo(capabilities.WideLines), yesNo(capabilities.IndependentBlend), yesNo(capabilities.SamplerAnisotropy),
			capabilities.MaxSamplerAnisotropy, yesNo(capabilities.DescriptorIndexing));
		HY_CORE_INFO("Renderer: limits: push constants {0} bytes, sampled images per stage {1}, depth D32F {2}, D24S8 {3}",
			capabilities.MaxPushConstantsSize, capabilities.MaxSampledImagesPerStage,
			yesNo(capabilities.DepthFormat32F), yesNo(capabilities.DepthFormat24Stencil8));
	}

	void Renderer::Init(Window& window)
	{
		HY_CORE_ASSERT(s_Instance == nullptr, "Renderer::Init called twice");

		s_Instance = new Renderer();
		Renderer& renderer = *s_Instance;

		renderer.m_Width = window.GetWidth();
		renderer.m_Height = window.GetHeight();

		// 1. Instance, surface, device.
		renderer.m_RendererContext = CreateScope<VulkanRendererContext>();
		renderer.m_RendererContext->Init(window);

		VulkanDevice& device = renderer.m_RendererContext->GetVulkanDevice();

		// 2. Swapchain.
		renderer.m_SwapChain = CreateScope<VulkanSwapChain>(device, renderer.m_RendererContext->GetSurface());
		if (renderer.m_SwapChain->Create(renderer.m_Width, renderer.m_Height, s_Settings.VSync) == false)
		{
			// Started minimised: build it on the first frame the window has a size.
			renderer.m_SwapChainDirty = true;
		}

		// What the GPU supports, for settings UI and for features that depend on it.
		{
			const VulkanPhysicalDevice& physicalDevice = device.GetPhysicalDevice();
			const VkPhysicalDeviceProperties& properties = physicalDevice.GetProperties();
			memcpy(renderer.m_GPUName, properties.deviceName, sizeof(renderer.m_GPUName));

			RendererCapabilities& capabilities = renderer.m_Capabilities;
			capabilities.GPUName = renderer.m_GPUName;
			capabilities.VulkanVersionMajor = VK_API_VERSION_MAJOR(properties.apiVersion);
			capabilities.VulkanVersionMinor = VK_API_VERSION_MINOR(properties.apiVersion);
			capabilities.VulkanVersionPatch = VK_API_VERSION_PATCH(properties.apiVersion);
			capabilities.FramesInFlight = MAX_FRAMES_IN_FLIGHT;

			capabilities.DynamicRendering = device.IsDynamicRenderingEnabled();
			capabilities.Synchronization2 = device.IsSynchronization2Enabled();
			capabilities.Wireframe = device.IsFillModeNonSolidEnabled();
			capabilities.WideLines = device.IsWideLinesEnabled();
			capabilities.IndependentBlend = device.IsIndependentBlendEnabled();
			capabilities.SamplerAnisotropy = device.IsSamplerAnisotropyEnabled();
			capabilities.DescriptorIndexing = physicalDevice.GetVulkan12Features().descriptorIndexing == VK_TRUE;

			capabilities.MaxPushConstantsSize = properties.limits.maxPushConstantsSize;
			capabilities.MaxSampledImagesPerStage = properties.limits.maxPerStageDescriptorSampledImages;
			capabilities.MaxSamplerAnisotropy = properties.limits.maxSamplerAnisotropy;
			capabilities.DepthFormat32F = physicalDevice.SupportsDepthAttachment(VK_FORMAT_D32_SFLOAT);
			capabilities.DepthFormat24Stencil8 = physicalDevice.SupportsDepthAttachment(VK_FORMAT_D24_UNORM_S8_UINT);

			LogCapabilities(capabilities);
		}

		// 3. Per-frame command buffers and sync objects.
		renderer.m_Frames.Init(device);

		// 4. GPU memory allocator.
		VulkanAllocator::Init(renderer.m_RendererContext->GetInstance(), device);
		// Descriptor set layouts and pools (persistent and per frame in flight).
		VulkanDescriptorAllocator::Init();

		// 5. Sub-renderers. Renderer2D draws into RenderViews the application creates.
		Renderer2D::Init();
		Renderer3D::Init();
	}

	void Renderer::Shutdown()
	{
		if (s_Instance == nullptr)
		{
			return;
		}

		WaitIdle();
		// What the layers released (render views, scenes and their textures) is freed now.
		s_Instance->FlushResourceFrees(true);

		// Exactly the reverse of Init.
		Renderer3D::Shutdown();
		Renderer2D::Shutdown();
		// The sub-renderers' resources went into the queue: free them while the allocator and device exist.
		s_Instance->FlushResourceFrees(true);
		VulkanDescriptorAllocator::Shutdown();
		VulkanAllocator::Shutdown();

		s_Instance->m_Frames.Shutdown();
		s_Instance->m_SwapChain->Shutdown();
		s_Instance->m_SwapChain.reset();
		s_Instance->m_RendererContext->Shutdown();
		s_Instance->m_RendererContext.reset();

		delete s_Instance;
		s_Instance = nullptr;
	}

	void Renderer::SetSettings(const RendererSettings& settings)
	{
		const bool vsyncChanged = settings.VSync != s_Settings.VSync;
		s_Settings = settings;

		if (s_Instance == nullptr)
		{
			return; // Init picks them up.
		}

		// Present mode is fixed per swapchain: rebuild it at the start of the next frame.
		// Clear colour, wireframe, hot reload and the lighting options are read by Renderer2D every frame.
		if (vsyncChanged)
		{
			s_Instance->m_SwapChain->SetVSync(settings.VSync);
			s_Instance->m_SwapChainDirty = true;
		}
	}

	void Renderer::WaitIdle()
	{
		if (s_Instance != nullptr && s_Instance->m_RendererContext != nullptr && s_Instance->m_RendererContext->HasVulkanDevice())
		{
			s_Instance->m_RendererContext->GetVulkanDevice().WaitIdle();
		}
	}

	void Renderer::SubmitResourceFree(ResourceFreeFunction freeFunction)
	{
		HY_CORE_ASSERT(s_Instance != nullptr, "Renderer::SubmitResourceFree after Renderer::Shutdown: a GPU resource outlived the renderer");
		if (s_Instance == nullptr)
		{
			// The device is gone, so there is nothing left to free it with. Never happens when every
			// owner releases its resources before Renderer::Shutdown (layers do it in OnDetach).
			HY_CORE_ERROR("A GPU resource was released after Renderer::Shutdown; its handles are leaked.");
			return;
		}

		PendingResourceFree pending;
		pending.Frame = s_Instance->m_FrameNumber;
		pending.Free = std::move(freeFunction);
		s_Instance->m_ResourceFreeQueue.push_back(std::move(pending));
	}

	void Renderer::FlushResourceFrees(bool all)
	{
		// BeginFrame has just waited for the fence of frame m_FrameNumber - MAX_FRAMES_IN_FLIGHT (the
		// last frame that used this slot), so that frame and every frame before it are finished.
		// Frees run in the order they were queued, so an owner that queues "children first"
		// (framebuffers before their images) keeps that order.
		while (m_ResourceFreeQueue.empty() == false)
		{
			PendingResourceFree& pending = m_ResourceFreeQueue.front();
			if (all == false && pending.Frame + MAX_FRAMES_IN_FLIGHT > m_FrameNumber)
			{
				break;
			}

			// Move it out first: a free function may release more resources (which queue more frees).
			ResourceFreeFunction freeResource = std::move(pending.Free);
			m_ResourceFreeQueue.pop_front();
			freeResource();
		}
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

		// Offscreen scene passes first (dynamic rendering). Their targets end in SHADER_READ_ONLY_OPTIMAL,
		// so the present pass below can sample them (the editor viewport does).
		Renderer2D::Render(commandBuffer);

		renderer->m_SwapChain->BeginRenderPass(commandBuffer, imageIndex);
		if (renderer->m_PresentPassCallback)
		{
			renderer->m_PresentPassCallback(commandBuffer);
		}
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
			{
				return false;
			}
		}

		m_Frames.WaitForFrame();
		// The frame that used this slot before is done, and so is every frame before it.
		FlushResourceFrees(false);
		// Its per-frame descriptor sets can be handed out again.
		VulkanDescriptorAllocator::ResetFrame(GetRenderFrame());

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
		VulkanDevice& device = m_RendererContext->GetVulkanDevice();

		m_Frames.Submit(device.GetGraphicsQueue(), m_SwapChain->GetRenderFinishedSemaphore(imageIndex));
		m_FrameNumber++;

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
		{
			return false; // Minimised.
		}

		m_RendererContext->GetVulkanDevice().WaitIdle();
		// The device is idle anyway: free everything that was waiting for a frame to finish.
		FlushResourceFrees(true);

		// Recreate also covers the case where the swapchain was never created (started minimised).
		if (m_SwapChain->Recreate(m_Width, m_Height) == false)
		{
			return false;
		}

		// RenderViews are sized by whoever shows them (the editor's windows), not the window, so they
		// are not touched here.
		m_SwapChainDirty = false;
		return true;
	}
}
