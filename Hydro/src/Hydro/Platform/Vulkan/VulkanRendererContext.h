#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Renderer/RendererContext.h"

namespace Hydro
{
	class VulkanRendererContext : public RendererContext
	{
	public:
		VulkanRendererContext();
		virtual ~VulkanRendererContext();

		virtual void Init() override;

	public:
		static VkInstance GetInstance() { return s_VulkanInstance; }
		bool CheckValidationLayerSupport();

	private:
		inline static VkInstance s_VulkanInstance;
		VkDebugReportCallbackEXT m_DebugReportCallback = VK_NULL_HANDLE;
	};
}


