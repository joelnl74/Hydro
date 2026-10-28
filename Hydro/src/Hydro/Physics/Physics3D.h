#pragma once
#include "Hydro/Physics/PhysicsScene3D.h"
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	class Scene;

	// What game code asks the 3D physics world of its scene (PhysicsScene3D). Everything answers "nothing" (false,
	// no entities) when the game isn't running or the scene has no 3D world.
	class Physics3D
	{
	public:
		static constexpr uint32_t LayerBit(PhysicsLayer layer) { return 1u << (uint32_t)layer; }
		static constexpr uint32_t AllLayers = 0xFFFFFFFFu;
		// Everything that blocks: all layers but Trigger.
		static constexpr uint32_t SolidLayers = AllLayers & ~(1u << (uint32_t)PhysicsLayer::Trigger);

		// The first collider on the segment from `from` to `to` on one of the layers in `layerMask`. A ray starting
		// inside a collider hits it at once (distance 0). Characters are not hit (they have no collider).
		static bool Raycast(Scene& scene, const glm::vec3& from, const glm::vec3& to, uint32_t layerMask, RaycastHit3D& hit);
		// Appends every entity whose collider on one of the layers in `layerMask` overlaps the sphere (no repeats).
		static void OverlapSphere(Scene& scene, const glm::vec3& centre, float radius, uint32_t layerMask, std::vector<Entity>& entities);
		// True when the entity has a collider body in its scene's running 3D world.
		static bool HasBody(Entity entity);
	};

	// A character of the 3D world (CharacterBodyComponent): game code says where it wants to go; the physics stage
	// walks it there over the ground and writes its transform.
	class CharacterBody
	{
	public:
		// The velocity the character wants, in metres per second along X and Y (Z is ignored). Kept until changed:
		// pass zero to stop. Call it every update from a script (stage 1). False when the entity has no character.
		static bool Move(Entity entity, const glm::vec3& velocity);
		// What the character stands on. False when the entity has no character.
		static bool GetGround(Entity entity, CharacterGround& ground);
		// True when the entity is a character in its scene's running 3D world.
		static bool IsCharacter(Entity entity);
	};
}
