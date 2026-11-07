#pragma once
#include <filesystem>
#include <vector>

namespace Hydro
{
	struct EngineResourcesSpecification
	{
		// Source folders (each with an assets/ folder) looked in before the copy next to the exe, so a development build
		// edits the real files (shader hot reload). Empty for packaged and Dist builds: they never depend on a checkout.
		std::vector<std::filesystem::path> DevelopmentDirectories;
	};

	// EngineResources subsystem: where the engine's and the application's own files (shaders, editor fonts) are, found
	// from the executable rather than the working directory. Every build copies them into <exe folder>/assets/.
	class EngineResources
	{
	public:
		static void Init(const EngineResourcesSpecification& specification);
		static void Shutdown();

		// "shaders/mesh.vert" -> the first <development directory>/assets/shaders/mesh.vert that exists, else
		// <exe folder>/assets/shaders/mesh.vert (logged when that is missing too). Absolute paths are returned as they are.
		static std::filesystem::path Resolve(const std::filesystem::path& relativePath);
		// <exe folder>/assets: the copy every build makes.
		static std::filesystem::path GetStagedDirectory();
	};
}
