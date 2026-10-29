#pragma once
#include "Entity.h"
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"

#include <filesystem>

namespace Hydro
{
	class Prefab;

	// Saves an entity and its whole subtree (with their components) as a reusable YAML prefab (.hyprefab)
	// and spawns copies of it into a scene.
	//
	// Handle: 9876543210   (the file's asset handle, see SceneSerializer)
	// Prefab: StreetLamp
	// Entities:            (the root first, then its subtree depth-first; older prefabs: one `Entity:` map)
	//   - Entity: 1234567890
	//     TagComponent:
	//       Tag: StreetLamp
	//     ...
	//   - Entity: 2345678901
	//     RelationshipComponent:
	//       Parent: 1234567890
	//     ...
	class PrefabSerializer
	{
	public:
		static bool Save(Entity entity, const std::filesystem::path& filepath);

		// Creates the prefab's entities in `scene` and returns its root. Every entity gets a new UUID; the links
		// between them follow (Scene::RemapEntityReferences), and the subtree keeps its shape. The root is a root
		// of the scene (last in its list), or the last child of `parent` when one is given; its saved transform
		// is then relative to that parent. Returns an invalid Entity ({}) on failure.
		static Entity Instantiate(Scene& scene, const std::filesystem::path& filepath, Entity parent = {});
		// From a loaded Prefab: no file is read, nothing is logged (for spawning often, e.g. Vfx::Play).
		static Entity Instantiate(Scene& scene, const Prefab& prefab, Entity parent = {});
		// The prefab asset `prefab`, loaded through the AssetManager when needed (reads its file the first
		// time and after it changed), then instantiated. Logs and returns {} when it can't be loaded.
		static Entity Instantiate(Scene& scene, AssetHandle prefab, Entity parent = {});

		// Same as Instantiate, but without logging success. For tools that read prefabs often,
		// such as the editor's project window building preview thumbnails.
		static Entity Load(Scene& scene, const std::filesystem::path& filepath, Entity parent = {});

		static constexpr const char* FileExtension = ".hyprefab";
	};
}
