#pragma once
#include "Entity.h"
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	class Prefab;

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
		// From a loaded Prefab: no file is read, nothing is logged (for spawning often, e.g. Vfx::Play).
		static Entity Instantiate(Scene& scene, const Prefab& prefab);
		// The prefab asset `prefab`, loaded through the AssetManager when needed (reads its file the first
		// time and after it changed), then instantiated. Logs and returns {} when it can't be loaded.
		static Entity Instantiate(Scene& scene, AssetHandle prefab);

		// Same as Instantiate, but without logging. For tools that read prefabs often,
		// such as the editor's project window building preview thumbnails.
		static Entity Load(Scene& scene, const std::filesystem::path& filepath);

		static constexpr const char* FileExtension = ".hyprefab";
	};
}
