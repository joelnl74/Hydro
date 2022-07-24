#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"

namespace Hydro
{
	class VulkanShader
	{
	public:
		VkShaderModule Create(const std::string& filePath);

	private:
		std::vector<char> readFile(const std::string& filePath);

	private:
		// TODO Make it a ref.
		VkShaderModule shaderModule;
	};
}


