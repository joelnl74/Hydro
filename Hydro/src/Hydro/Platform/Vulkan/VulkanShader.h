#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"

namespace Hydro
{
	class VulkanShader
	{
	public:
		VkShaderModule Create(const std::string& filePath);
		void Destory();

		VkShaderModule & GetModule() { return m_ShaderModule; }

		void CreateDescriptorSetLayout();

	private:
		std::vector<char> readFile(const std::string& filePath);

	private:
		// TODO Make it a ref.
		VkDescriptorSetLayout m_DescriptorSetLayout;
		VkShaderModule m_ShaderModule;
	};
}


