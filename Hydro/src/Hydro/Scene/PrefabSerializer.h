#pragma once
#include "Entity.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	// Saves a single entity (with its components) as a reusable YAML prefab (.hyprefab)
	// and spawns copies of it into a scene.
	//
	// Handle: 9876543210   (the file's asset handle, see SceneSerializer)
	// Prefab: Player
	// Entity:
	//   Entity: 1234567890
	//   TagComponent:
	//     Tag: Player
	//   ...
	class PrefabSerializer
	{
	public:
		static bool Save(Entity entity, const std::filesystem::path& filepath);

		// Creates a new entity in `scene` from the prefab. Each instance gets its own UUID.
		// Returns an invalid Entity ({}) on failure.
		static Entity Instantiate(Scene& scene, const std::filesystem::path& filepath);

		// Same as Instantiate, but without logging. For tools that read prefabs often,
		// such as the editor's project window building preview thumbnails.
		static Entity Load(Scene& scene, const std::filesystem::path& filepath);

		static constexpr const char* FileExtension = ".hyprefab";
	};
}
