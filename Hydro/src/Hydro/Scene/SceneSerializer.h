#pragma once
#include "Scene.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace YAML
{
	class Emitter;
	class Node;
}

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

		// The scene's render settings, Look, environment and post-processing: keys written into an open map (only those that
		// differ from the defaults), and read back from one (missing keys give the defaults). The editor's undo uses both.
		static void SerializeSettings(YAML::Emitter& out, const Scene& scene);
		static void DeserializeSettings(const YAML::Node& data, Scene& scene);

	private:
		Ref<Scene> m_Scene;
	};
}
