#pragma once
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>
#include <entt.hpp>

#include <cstdint>
#include <vector>

namespace Hydro
{
	enum class ColliderShapeType : uint8_t
	{
		Box = 0,
		Sphere,
		Capsule,  // along Z
		Cylinder, // along Z
		Prism,    // a convex solid from two rings of points (Points)
		ConvexHull,  // the convex hull of Points (a model's _COL node); Indices are its triangles, for outlines
		TriangleMesh // Points and Indices as triangles, hollow: walkable ground (a model's _WALK node)
	};

	// One collision shape in the entity's own space with its scale applied (the body adds rotation and position).
	// Both PhysicsWorld3D and the editor's outlines use it, so they always agree.
	struct ColliderShape
	{
		ColliderShapeType Type = ColliderShapeType::Box;
		glm::vec3 Centre{ 0.0f };
		glm::vec3 HalfExtents{ 0.5f }; // Box
		float Radius = 0.5f;           // Sphere, Capsule, Cylinder
		float HalfHeight = 0.5f;       // Capsule: half of its straight part; Cylinder: half of its height
		// Prism: two rings of the same size, point i joined to point i; the solid is their convex hull.
		std::vector<glm::vec3> Points;
		std::vector<uint32_t> Indices; // ConvexHull, TriangleMesh: triangles into Points
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// An entity's 3D collider components (and a terrain's collision mesh) as shapes.
	class ColliderShapes
	{
	public:
		// The smallest half size of a shape, so a flat box still collides.
		static constexpr float MinHalfSize = 0.01f;
		// How thick a Plane's collision slab is (under its surface).
		static constexpr float PlaneThickness = 0.1f;

		// True when the entity has any 3D collider component.
		static bool HasColliders(entt::registry& registry, entt::entity entity);

		// Appends the entity's shapes sized by its world `scale`. False when a mesh collider's model isn't loaded yet
		// (that shape is missing, ask again later).
		static bool Get(entt::registry& registry, entt::entity entity, const glm::vec3& scale, std::vector<ColliderShape>& shapes);
	};
}
