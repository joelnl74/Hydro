#pragma once
#include <vulkan/vulkan.h>
#include "Hydro/Core/Base.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Hydro
{
	enum class ShaderType
	{
		None = 0,
		Vertex = 1,
		Fragment = 2
	};

	struct ShaderInformation
	{
		// GLSL (.vert, .frag, .comp, ...) compiled with shaderc, or SPIR-V (.spv) as is. GLSL may #include "file"
		// relative to itself.
		std::string path;
		VkShaderStageFlagBits shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
		ShaderType shaderType = ShaderType::None;
	};

	struct ShaderSpecification
	{
		std::vector<ShaderInformation> shaderInformation;
	};

	// One VkShaderModule per stage. Throws std::runtime_error (with the compiler's message) when a stage fails;
	// nothing leaks then.
	class VulkanShader
	{
	public:
		explicit VulkanShader(const ShaderSpecification& spec);
		~VulkanShader();

		VulkanShader(const VulkanShader&) = delete;
		VulkanShader& operator=(const VulkanShader&) = delete;

		VkShaderModule& GetModule(ShaderType type)
		{
			return m_ShaderModules[(uint32_t)type];
		}

		const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInformation() const { return m_ShaderStages; }
		const ShaderSpecification& GetSpecification() const { return m_Specification; }

		// Each stage's source plus the files it #includes: watched for hot reload.
		const std::vector<std::filesystem::path>& GetSourceFiles() const { return m_SourceFiles; }

	private:
		void CreateStage(const ShaderInformation& info);
		std::vector<uint32_t> LoadSpirv(const ShaderInformation& info);
		std::vector<uint32_t> CompileGlsl(const ShaderInformation& info);
		void DestroyModules();

	private:
		ShaderSpecification m_Specification;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
		std::map<uint32_t, VkShaderModule> m_ShaderModules;
		std::vector<std::filesystem::path> m_SourceFiles;
	};
}
