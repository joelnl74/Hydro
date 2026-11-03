#pragma once
#include "Entity.h"
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	class Prefab;

	// Saves an entity and its subtree as a YAML prefab (.hyprefab: `Entities:`, the root first, depth-first) and spawns
	// copies of it. Format: docs/SceneAndECS.md.
	class PrefabSerializer
	{
	public:
		static bool Save(Entity entity, const std::filesystem::path& filepath);

		// Creates the prefab's entities with new UUIDs (links remapped) and returns the root: a scene root, or the last
		// child of `parent` (its saved transform then relative to it). Empty on failure.
		static Entity Instantiate(Scene& scene, const std::filesystem::path& filepath, Entity parent = {});
		// From a loaded Prefab: no file read, nothing logged (for spawning often, e.g. Vfx::Play).
		static Entity Instantiate(Scene& scene, const Prefab& prefab, Entity parent = {});
		// Loads the prefab asset through the AssetManager when needed, then instantiates it. Logs and returns {} on failure.
		static Entity Instantiate(Scene& scene, AssetHandle prefab, Entity parent = {});

		// Instantiate without logging success, for tools that read prefabs often (thumbnails).
		static Entity Load(Scene& scene, const std::filesystem::path& filepath, Entity parent = {});

		static constexpr const char* FileExtension = ".hyprefab";
	};
}
