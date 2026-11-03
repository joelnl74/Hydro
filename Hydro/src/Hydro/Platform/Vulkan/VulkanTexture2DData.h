#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanImage.h"

namespace Hydro
{
	// The Vulkan side of a Texture2D, kept behind a pointer so Texture2D.h has no Vulkan types.
	struct Texture2DData
	{
		Scope<VulkanImage> Image;
	};

	class Texture2D;

	// A friend of Texture2D, defined in Texture2D.cpp.
	const Texture2DData& GetTexture2DData(const Texture2D& texture);
}
