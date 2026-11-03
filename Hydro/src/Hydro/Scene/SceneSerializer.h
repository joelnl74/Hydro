#pragma once
#include "Scene.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	// Saves and loads a whole scene as YAML (.hyscene: Handle, Scene, Entities). The handle line is kept by
	// AssetManager::ImportAsset when the file is saved again; loading ignores it.
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);

		static constexpr const char* FileExtension = ".hyscene";

	private:
		Ref<Scene> m_Scene;
	};
}
