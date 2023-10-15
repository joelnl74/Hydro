#include <hypch.h>

#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"

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
		s_Instance->m_vulkanPresentation->StartShutDown();

		Renderer2D::ShutDown();
		VulkanAllocator::Shutdown();
		s_Instance->m_vulkanPresentation->ShutDown();
		s_Instance->m_rendererContext->ShutDown();

		delete s_Instance;
	}

	void Renderer::Init(Window& window)
	{
		s_Instance->m_rendererContext = CreateRef<VulkanRendererContext>();
		s_Instance->m_vulkanPresentation = CreateRef<VulkanSwapChain>(window);

		s_Instance->m_rendererContext->Init();
		s_Instance->m_vulkanPresentation->Init(s_Instance->m_rendererContext->GetInstance(), s_Instance->m_rendererContext->GetVulkanDevice());
		s_Instance->m_vulkanPresentation->InitSurface(window);
		s_Instance->m_vulkanPresentation->CreateSwapChain(window, true);

		VulkanAllocator::Init(s_Instance->m_rendererContext->GetVulkanDevice());
		// Renderer2D::Init();
		Renderer3D::Init();
	}
}
