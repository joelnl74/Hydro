#include <hypch.h>

#include "RendererContext.h"

#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"

namespace Hydro
{
	Scope<RendererContext> RendererContext::Create()
	{
		return Hydro::CreateScope<VulkanRendererContext>();
	}
}
