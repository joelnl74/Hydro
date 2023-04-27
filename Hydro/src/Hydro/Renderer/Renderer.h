#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanSwapChain.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

#include "Hydro/Core/Window.h"

namespace Hydro
{

	class Renderer
	{
	public:
		static void Create(Window &window);
		static void RenderFrame() { s_Instance->m_vulkanPresentation->DrawFrame(); }

		static uint32_t GetRenderFrame() { return s_Instance->m_vulkanPresentation->GetRenderFrame(); }
		static Ref<VulkanRendererContext> GetRendererContext() { return s_Instance->m_rendererContext; }
		static Ref<VulkanSwapChain> GetVulkanSwapChain() { return s_Instance->m_vulkanPresentation; }

		static void Init(Window& window);
		static void ShutDown();

	private:
		static Renderer *s_Instance;

		Ref<VulkanRendererContext> m_rendererContext;
		Ref<VulkanSwapChain> m_vulkanPresentation;
	};

}
