#include <hypch.h>

#include "RendererContext.h"

#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"

namespace Hydro
{
	Ref<RendererContext> RendererContext::s_Instance = nullptr;

	Ref<RendererContext> RendererContext::Create()
	{
		s_Instance = Hydro::CreateRef<VulkanRendererContext>();

		return  s_Instance;
	}
}
