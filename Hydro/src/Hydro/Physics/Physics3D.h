#pragma once
#include "Hydro/Physics/PhysicsScene3D.h"
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class Scene;

	// Game code's queries of its scene's 3D world. Everything answers "nothing" when the game isn't running.
	class Physics3D
	{
	public:
		static constexpr uint32_t LayerBit(PhysicsLayer layer) { return 1u << (uint32_t)layer; }
		static constexpr uint32_t AllLayers = 0xFFFFFFFFu;
		// Everything that blocks: all layers but Trigger.
		static constexpr uint32_t SolidLayers = AllLayers & ~(1u << (uint32_t)PhysicsLayer::Trigger);

		// The first collider on the segment, on the layers in `layerMask`; one containing the start hits at distance 0.
		// Characters have no collider and are not hit.
		static bool Raycast(Scene& scene, const glm::vec3& from, const glm::vec3& to, uint32_t layerMask,
			RaycastHit3D& hit);
		// Appends the entities whose collider on those layers overlaps the sphere, without repeats.
		static void OverlapSphere(Scene& scene, const glm::vec3& centre, float radius, uint32_t layerMask,
			std::vector<Entity>& entities);
		// A collider body in its scene's running 3D world.
		static bool HasBody(Entity entity);
	};

	// A character of the 3D world: game code says where it wants to go; the physics stage walks it there.
	class CharacterBody
	{
	public:
		// Metres per second along X and Y, kept until changed (zero stops). Call it from a script's update.
		static bool Move(Entity entity, const glm::vec3& velocity);
		// False when the entity has no character.
		static bool GetGround(Entity entity, CharacterGround& ground);
		static bool IsCharacter(Entity entity);
	};
}
