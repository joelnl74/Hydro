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

	// One collision shape of an entity, in the entity's own space with its scale already applied: the body adds the
	// transform's rotation and position. What PhysicsScene3D turns into a Jolt shape and what the editor outlines,
	// so the two always agree.
	struct ColliderShape
	{
		ColliderShapeType Type = ColliderShapeType::Box;
		glm::vec3 Centre{ 0.0f };
		glm::vec3 HalfExtents{ 0.5f }; // Box
		float Radius = 0.5f;           // Sphere, Capsule, Cylinder
		float HalfHeight = 0.5f;       // Capsule: half of its straight part; Cylinder: half of its height
		// Prism: two rings of the same size; point i of the first ring is joined to point i of the second. The solid
		// is their convex hull, so the rings describe a convex shape (a ramp, the stairs' ramp, an oval post).
		std::vector<glm::vec3> Points;
		std::vector<uint32_t> Indices; // ConvexHull, TriangleMesh: triangles into Points
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// The 3D collider components of an entity as shapes (BoxColliderComponent, SphereColliderComponent,
	// CapsuleColliderComponent, CylinderColliderComponent, MeshColliderComponent), and a TerrainComponent's terrain (one
	// TriangleMesh: Terrain::GetCollisionMesh).
	class ColliderShapes
	{
	public:
		// The smallest half size of a shape, so a flat box still collides.
		static constexpr float MinHalfSize = 0.01f;
		// How thick a Plane's collision slab is (under its surface).
		static constexpr float PlaneThickness = 0.1f;

		// True when the entity has any 3D collider component.
		static bool HasColliders(entt::registry& registry, entt::entity entity);

		// Appends the entity's shapes, sized by `scale`: the entity's scale in the world (WorldTransformComponent::Scale,
		// or Scene::GetWorldTransform's when it must be current), so a child of a scaled parent collides at its drawn
		// size. Returns false when a MeshColliderComponent names a model that isn't loaded (AssetManager): that shape
		// is missing, ask again later. An entity without a TransformComponent has none.
		static bool Get(entt::registry& registry, entt::entity entity, const glm::vec3& scale, std::vector<ColliderShape>& shapes);
	};
}
