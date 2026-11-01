#pragma once

#include <glm/glm.hpp>

namespace Hydro
{
	class Entity;
	struct MeshComponent;

	// The box around a mesh entity (MeshComponent): its model's bounds or its shape's, in its own space or around it
	// in the world. One place for what the editor's selection outline and game code (the camera's occluder fade,
	// Game/src/Scripts/Common/OccluderFadeScript.h) both need.
	class MeshBounds
	{
	public:
		// The mesh's box in its own space. False when there is no mesh to measure yet (a model that isn't loaded).
		static bool GetLocal(const MeshComponent& mesh, glm::vec3& low, glm::vec3& high);
		// The axis-aligned world box around the mesh's eight turned corners, from the entity's WorldTransformComponent
		// (as of the last Scene::ResolveTransforms). False when the entity has no MeshComponent or no mesh yet.
		static bool GetWorld(Entity entity, glm::vec3& low, glm::vec3& high);
	};
}
