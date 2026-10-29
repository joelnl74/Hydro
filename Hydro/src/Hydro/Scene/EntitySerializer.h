#pragma once
#include "Entity.h"

#include "Hydro/Utils/YAMLConversions.h"

// Shared YAML encoding for entities and their components.
// Used by both SceneSerializer (.hyscene) and PrefabSerializer (.hyprefab),
// so a new component only has to be added in EntitySerializer.cpp.

namespace Hydro
{
	class EntitySerializer
	{
	public:
		// Writes one entity as a YAML map: { Entity: <uuid>, TagComponent: ..., TransformComponent: ..., ... }
		// Without includeParent its RelationshipComponent is left out (a prefab's root: its parent isn't in the file).
		static void Serialize(YAML::Emitter& out, Entity entity, bool includeParent = true);

		// Creates an entity in `scene` from a map written by Serialize().
		// keepUUID = true  -> reuse the saved UUID (loading a scene).
		// keepUUID = false -> give it a fresh UUID (instantiating a prefab, so copies don't clash).
		static Entity Deserialize(const YAML::Node& entityNode, Scene& scene, bool keepUUID);
	};
}
