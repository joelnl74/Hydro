#pragma once
#include "VulkanShader.h"

#include <string>
#include <vector>

namespace Hydro
{
	// The renderer's shaders by name, each a compiled vertex + fragment pair, and their hot reload. Pipelines don't
	// keep a shader, so whoever built one rebuilds it when ReloadChanged names its shader.
	class VulkanShaderLibrary
	{
	public:
		static void Init();
		static void Shutdown();

		// Compiles the pair under `name`, replacing a shader of that name. Relative paths are engine resources
		// ("shaders/mesh.vert", EngineResources::Resolve). Throws std::runtime_error when it doesn't
		// compile (VulkanShader logs the output).
		static VulkanShader& Load(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
		static VulkanShader* Get(const std::string& name);

		// Recompiles shaders whose files (or #includes) changed; returns the names whose new version took over. A
		// shader that doesn't compile keeps its old version (logged).
		static std::vector<std::string> ReloadChanged();
	};
}
