#pragma once

#include <glm/glm.hpp>

namespace Hydro
{
	class Entity;
	struct MeshComponent;

	// The box around a mesh entity, local or in the world, for the editor's selection outline and game code (the
	// camera's occluder fade).
	class MeshBounds
	{
	public:
		// The mesh's box in its own space. False when there is no mesh to measure yet (a model that isn't loaded).
		static bool GetLocal(const MeshComponent& mesh, glm::vec3& low, glm::vec3& high);
		// The world box around the mesh's turned corners, as of the last ResolveTransforms. False without a mesh yet.
		static bool GetWorld(Entity entity, glm::vec3& low, glm::vec3& high);
	};
}
