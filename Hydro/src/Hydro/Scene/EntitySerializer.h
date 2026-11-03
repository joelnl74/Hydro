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
	};
}
