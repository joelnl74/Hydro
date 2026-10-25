#pragma once
#include "VulkanShader.h"

#include <string>
#include <vector>

namespace Hydro
{
	// The renderer's shaders by name ("Sprite", "SpriteCutout", "Light", "Shadow", "Composite", later "Mesh"),
	// each a vertex + fragment pair compiled from GLSL, and their hot reload in one place (plan M4a). Started
	// and stopped by Renderer::Init / Shutdown. Pipelines don't keep a shader (PipelineSpecification::Shader
	// is only read at creation), so whoever built a pipeline from a shader rebuilds it when ReloadChanged
	// names that shader.
	class VulkanShaderLibrary
	{
	public:
		static void Init();
		static void Shutdown();

		// Compiles the pair and keeps it under `name`, replacing a shader of that name. Throws
		// std::runtime_error when it doesn't compile (VulkanShader logs the compiler's output).
		static VulkanShader& Load(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
		// The shader loaded under `name`, or null.
		static VulkanShader* Get(const std::string& name);

		// Polls every loaded shader's files (its stages and what they #include) and compiles the changed ones
		// again. Returns the names whose new version took over. A shader that doesn't compile keeps its old
		// version (logged) and is tried again on its next change.
		static std::vector<std::string> ReloadChanged();
	};
}
