#include <hypch.h>

#include "RendererContext.h"

#include "Hydro/Platform/Vulkan/VulkanRendererContext.h"

namespace Hydro
{
	Ref<RendererContext> RendererContext::Create()
	{
		return Hydro::CreateRef<VulkanRendererContext>();
	}
}
