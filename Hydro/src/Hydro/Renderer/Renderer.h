#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"
#include "Hydro/Platform/Vulkan/VulkanSwapChain.h"
#include "Hydro/Platform/Vulkan/VulkanAllocator.h"

#include "Hydro/Core/Window.h"

namespace Hydro
{

	class Renderer
	{
	public:
		static Renderer& Get() { return *s_Instance; }
		static void Create(Window &window);
		static void RenderFrame() { Get().m_vulkanPresentation->DrawFrame(); }

		static uint32_t GetRenderFrame() { return Get().m_vulkanPresentation->GetRenderFrame(); }
		static Ref<VulkanRendererContext> GetRendererContext() { return Get().m_rendererContext; }
		static Ref<VulkanSwapChain> GetVulkanSwapChain() { return Get().m_vulkanPresentation; }

		void Init(Window& window);
		void ShutDown();

	private:
		static Renderer *s_Instance;

		Ref<VulkanRendererContext> m_rendererContext;
		Ref<VulkanSwapChain> m_vulkanPresentation;
	};

}
