#include "hypch.h"
#include "VulkanShaderLibrary.h"

#include "Hydro/Core/FileWatcher.h"

#include <map>

namespace Hydro
{
	struct LibraryShader
	{
		Scope<VulkanShader> Shader;
		FileWatcher Watcher;
	};

	struct VulkanShaderLibraryData
	{
		std::map<std::string, LibraryShader> Shaders;
	};

	static VulkanShaderLibraryData* s_Library = nullptr;

	static ShaderSpecification MakeSpecification(const std::string& vertexPath, const std::string& fragmentPath)
	{
		ShaderInformation vertex;
		vertex.path = vertexPath;
		vertex.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
		vertex.shaderType = ShaderType::Vertex;

		ShaderInformation fragment;
		fragment.path = fragmentPath;
		fragment.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragment.shaderType = ShaderType::Fragment;

		ShaderSpecification specification;
		specification.shaderInformation = { vertex, fragment };
		return specification;
	}

	void VulkanShaderLibrary::Init()
	{
		HY_CORE_ASSERT(s_Library == nullptr, "VulkanShaderLibrary::Init called twice");
		s_Library = new VulkanShaderLibraryData();
	}

	void VulkanShaderLibrary::Shutdown()
	{
		// Shader modules are only needed while pipelines are created, so they can go at once.
		delete s_Library;
		s_Library = nullptr;
	}

	VulkanShader& VulkanShaderLibrary::Load(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath)
	{
		HY_CORE_ASSERT(s_Library != nullptr, "VulkanShaderLibrary::Load before Init");

		// Compiled first: a shader that doesn't compile throws and leaves the library as it was.
		Scope<VulkanShader> shader = CreateScope<VulkanShader>(MakeSpecification(vertexPath, fragmentPath));
		LibraryShader& entry = s_Library->Shaders[name];
		entry.Watcher = FileWatcher(shader->GetSourceFiles());
		entry.Shader = std::move(shader);
		return *entry.Shader;
	}

	VulkanShader* VulkanShaderLibrary::Get(const std::string& name)
	{
		if (s_Library == nullptr)
		{
			return nullptr;
		}
		auto it = s_Library->Shaders.find(name);
		return it == s_Library->Shaders.end() ? nullptr : it->second.Shader.get();
	}

	std::vector<std::string> VulkanShaderLibrary::ReloadChanged()
	{
		std::vector<std::string> reloaded;
		if (s_Library == nullptr)
		{
			return reloaded;
		}

		for (auto& [name, entry] : s_Library->Shaders)
		{
			// Every watcher is polled (a poll takes the new file times); a broken shader is retried on its
			// next save, not on every poll.
			if (entry.Watcher.Poll() == false)
			{
				continue;
			}

			Scope<VulkanShader> shader;
			try
			{
				shader = CreateScope<VulkanShader>(entry.Shader->GetSpecification());
			}
			catch (const std::exception&)
			{
				HY_CORE_WARN("Shader '{}' doesn't compile; still using the previous version.", name);
				continue;
			}

			// Its files may have changed (a new #include): watch the new list.
			entry.Watcher = FileWatcher(shader->GetSourceFiles());
			entry.Shader = std::move(shader);
			reloaded.push_back(name);
		}
		return reloaded;
	}
}
