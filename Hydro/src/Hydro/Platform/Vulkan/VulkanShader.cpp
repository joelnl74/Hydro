#include "hypch.h"
#include "VulkanShader.h"

#include "VulkanRendererContext.h"
#include "VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

#include <shaderc/shaderc.hpp>
#include <stdexcept>

namespace Hydro
{
	static bool ReadTextFile(const std::filesystem::path& path, std::string& outText)
	{
		std::ifstream file(path, std::ios::in | std::ios::binary);
		if (file.is_open() == false)
			return false;

		std::stringstream stream;
		stream << file.rdbuf();
		outText = stream.str();
		return true;
	}

	static shaderc_shader_kind ShaderStageToShadercKind(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:                  return shaderc_glsl_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:                return shaderc_glsl_fragment_shader;
		case VK_SHADER_STAGE_COMPUTE_BIT:                 return shaderc_glsl_compute_shader;
		case VK_SHADER_STAGE_GEOMETRY_BIT:                return shaderc_glsl_geometry_shader;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return shaderc_glsl_tess_control_shader;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return shaderc_glsl_tess_evaluation_shader;
		default: break;
		}

		throw std::runtime_error("Unsupported shader stage for GLSL compilation.");
	}

	// Resolves #include "file" (and <file>) relative to the file that includes it, and records
	// every included path so hot reload also triggers when only an included file changes.
	class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
	{
	public:
		explicit ShaderIncluder(std::vector<std::filesystem::path>* includedFiles)
			: m_IncludedFiles(includedFiles)
		{
		}

		shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type /*type*/, const char* requestingSource, size_t /*includeDepth*/) override
		{
			auto* include = new IncludeData();
			const std::filesystem::path path = (std::filesystem::path(requestingSource).parent_path() / requestedSource).lexically_normal();
			m_IncludedFiles->push_back(path);

			if (ReadTextFile(path, include->Content))
			{
				include->Name = path.generic_string();
			}
			else
			{
				// shaderc's convention: an empty source name means failure; the content is the error message.
				include->Content = "Could not open include file '" + path.generic_string() + "'";
			}

			include->Result.source_name = include->Name.c_str();
			include->Result.source_name_length = include->Name.size();
			include->Result.content = include->Content.c_str();
			include->Result.content_length = include->Content.size();
			include->Result.user_data = include;
			return &include->Result;
		}

		void ReleaseInclude(shaderc_include_result* result) override
		{
			delete static_cast<IncludeData*>(result->user_data);
		}

	private:
		struct IncludeData
		{
			shaderc_include_result Result{};
			std::string Name;
			std::string Content;
		};

		std::vector<std::filesystem::path>* m_IncludedFiles = nullptr;
	};

	VulkanShader::VulkanShader(const ShaderSpecification& spec)
		: m_Specification(spec)
	{
		try
		{
			for (const auto& shaderInfo : spec.shaderInformation)
			{
				CreateStage(shaderInfo);
			}
		}
		catch (...)
		{
			// The destructor does not run when the constructor throws: free the stages built so far.
			DestroyModules();
			throw;
		}
	}

	VulkanShader::~VulkanShader()
	{
		DestroyModules();
	}

	void VulkanShader::CreateStage(const ShaderInformation& info)
	{
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();

		m_SourceFiles.push_back(std::filesystem::path(info.path).lexically_normal());

		const bool isSpirv = std::filesystem::path(info.path).extension() == ".spv";
		std::vector<uint32_t> code = isSpirv ? LoadSpirv(info) : CompileGlsl(info);

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size() * sizeof(uint32_t);
		createInfo.pCode = code.data();

		VkShaderModule shaderModule = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = info.shaderStage;
		shaderStageInfo.module = shaderModule;
		shaderStageInfo.pName = "main";

		m_ShaderModules.insert({(uint32_t)info.shaderType, shaderModule });
		m_ShaderStages.push_back(shaderStageInfo);
	}

	std::vector<uint32_t> VulkanShader::LoadSpirv(const ShaderInformation& info)
	{
		std::ifstream file(info.path, std::ios::ate | std::ios::binary);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("Failed to open shader '{0}'", info.path);
			throw std::runtime_error("Failed to open shader '" + info.path + "'");
		}

		const size_t fileSize = (size_t)file.tellg();
		if (fileSize == 0 || fileSize % sizeof(uint32_t) != 0)
		{
			HY_CORE_ERROR("'{0}' is not a valid SPIR-V file", info.path);
			throw std::runtime_error("'" + info.path + "' is not a valid SPIR-V file");
		}

		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
		return buffer;
	}

	std::vector<uint32_t> VulkanShader::CompileGlsl(const ShaderInformation& info)
	{
		const std::string sourceName = std::filesystem::path(info.path).lexically_normal().generic_string();

		std::string source;
		if (ReadTextFile(info.path, source) == false)
		{
			HY_CORE_ERROR("Failed to open shader '{0}'", info.path);
			throw std::runtime_error("Failed to open shader '" + info.path + "'");
		}

		shaderc::CompileOptions options;
		options.SetSourceLanguage(shaderc_source_language_glsl);
		// Matches the instance's apiVersion (VulkanRendererContext).
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
		options.SetIncluder(std::make_unique<ShaderIncluder>(&m_SourceFiles));
		for (const auto& [name, value] : m_Specification.Defines)
		{
			options.AddMacroDefinition(name, value);
		}
#ifdef HY_DEBUG
		// Keeps names and line info so RenderDoc / validation messages point at the GLSL.
		options.SetGenerateDebugInfo();
#else
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

		shaderc::Compiler compiler;
		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, ShaderStageToShadercKind(info.shaderStage), sourceName.c_str(), "main", options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			HY_CORE_ERROR("Shader compilation failed:\n{0}", result.GetErrorMessage());
			throw std::runtime_error(result.GetErrorMessage());
		}

		if (result.GetNumWarnings() > 0)
			HY_CORE_WARN("Shader '{0}' compiled with warnings:\n{1}", sourceName, result.GetErrorMessage());

		return std::vector<uint32_t>(result.cbegin(), result.cend());
	}

	void VulkanShader::DestroyModules()
	{
		if (m_ShaderModules.empty())
			return;

		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		for (auto& module : m_ShaderModules)
		{
			vkDestroyShaderModule(device, module.second, nullptr);
		}

		m_ShaderModules.clear();
		m_ShaderStages.clear();
	}
}
