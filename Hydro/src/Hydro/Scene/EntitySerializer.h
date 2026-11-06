#pragma once
#include "Entity.h"

#include "Hydro/Utils/YAMLConversions.h"

// The YAML of entities and their components, shared by SceneSerializer and PrefabSerializer, so a new component
// is only added in EntitySerializer.cpp.

namespace Hydro
{
	class EntitySerializer
	{
	public:
		// Writes one entity as a YAML map. Without includeParent its parent is left out (a prefab's root).
		static void Serialize(YAML::Emitter& out, Entity entity, bool includeParent = true);

		// Creates an entity from a map Serialize() wrote. keepUUID reuses the saved UUID (scenes); otherwise it gets a
		// fresh one (prefab instances).
		static Entity Deserialize(const YAML::Node& entityNode, Scene& scene, bool keepUUID);

		// Gives an existing entity the name and components of a map Serialize() wrote, removing the ones the map hasn't.
		// Its UUID, handle, parent and place in the hierarchy stay (the editor's undo).
		static void ReplaceComponents(const YAML::Node& entityNode, Entity entity);

	private:
		// The engine's and the game's components and the scripts of the map, onto an entity that has none of them.
		static void LoadComponents(const YAML::Node& entityNode, Entity entity, const std::string& name);
	};
}
