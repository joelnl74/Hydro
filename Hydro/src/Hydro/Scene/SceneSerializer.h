#pragma once
#include "Scene.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	// Saves / loads a whole scene as YAML (.hyscene).
	//
	// Scene: Untitled
	// Entities:
	//   - Entity: 1234567890
	//     TagComponent:
	//       Tag: Player
	//     TransformComponent:
	//       Translation: [0, 0, 0]
	//       ...
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
