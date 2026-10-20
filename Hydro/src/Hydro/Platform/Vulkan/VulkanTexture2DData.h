#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Platform/Vulkan/VulkanImage.h"

namespace Hydro
{
	// The Vulkan side of a Texture2D (Renderer/Texture2D.h keeps it behind a pointer so the engine-facing
	// header has no Vulkan types). Only Texture2D.cpp and Renderer2D.cpp include this.
	struct Texture2DData
	{
		Scope<VulkanImage> Image;
	};

	class Texture2D;

	// The backend data of a texture (a friend of Texture2D, defined in Texture2D.cpp).
	const Texture2DData& GetTexture2DData(const Texture2D& texture);
}
