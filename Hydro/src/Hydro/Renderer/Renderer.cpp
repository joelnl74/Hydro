#include <hypch.h>

#include "Renderer.h"

namespace Hydro
{
	Renderer* Renderer::s_Instance = nullptr;

	void Renderer::Create(Window& window)
	{
		s_Instance = new Renderer();

		s_Instance->Init(window);
	}

	void Renderer::ShutDown()
	{
		m_vulkanPresentation->ShutDown();
		m_rendererContext->ShutDown();
	}

	void Renderer::Init(Window& window)
	{
		m_rendererContext = CreateRef<VulkanRendererContext>();
		m_vulkanPresentation = CreateRef<VulkanSwapChain>(window);

		m_rendererContext->Init();
		m_vulkanPresentation->Init(m_rendererContext->GetInstance(), m_rendererContext->GetVulkanDevice());
		m_vulkanPresentation->InitSurface(window);
		m_vulkanPresentation->CreateSwapChain(window, true);

		VulkanAllocator::Init(m_rendererContext->GetVulkanDevice());
	}
}
