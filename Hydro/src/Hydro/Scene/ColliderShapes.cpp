#include "hypch.h"
#include "ColliderShapes.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Terrain/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	static ColliderShape MakeBox(const glm::vec3& centre, const glm::vec3& halfExtents)
	{
		ColliderShape shape;
		shape.Type = ColliderShapeType::Box;
		shape.Centre = centre;
		shape.HalfExtents = glm::max(halfExtents, glm::vec3(ColliderShapes::MinHalfSize));
		return shape;
	}

	// A prism along X from a profile in the YZ plane (points (y, z)), from x0 to x1.
	static ColliderShape MakeProfilePrism(const std::vector<glm::vec2>& profile, float x0, float x1)
	{
		ColliderShape shape;
		shape.Type = ColliderShapeType::Prism;
		for (const glm::vec2& point : profile)
		{
			shape.Points.push_back({ x0, point.x, point.y });
		}
		for (const glm::vec2& point : profile)
		{
			shape.Points.push_back({ x1, point.x, point.y });
		}
		return shape;
	}

	// A built-in shape of a MeshComponent (MeshFactory: Z up, standing on the entity's position), scaled.
	static ColliderShape MakeMeshShape(const MeshComponent& mesh, const glm::vec3& scale)
	{
		const glm::vec3 size = glm::max(glm::abs(mesh.Size) * scale, glm::vec3(2.0f * ColliderShapes::MinHalfSize));
		const float x0 = -0.5f * size.x;
		const float x1 = 0.5f * size.x;
		const float y0 = -0.5f * size.y;
		const float y1 = 0.5f * size.y;
		const float h = size.z;

		switch (mesh.Shape)
		{
			case MeshShape::Plane:
			{
				const float half = 0.5f * ColliderShapes::PlaneThickness;
				return MakeBox({ 0.0f, 0.0f, -half }, { 0.5f * size.x, 0.5f * size.y, half });
			}
			case MeshShape::Ramp:
			{
				// From the front (-Y) edge on the ground up to the back edge at full height (MeshFactory::Ramp).
				return MakeProfilePrism({ { y0, 0.0f }, { y1, h }, { y1, 0.0f } }, x0, x1);
			}
			case MeshShape::Stairs:
			{
				// A ramp through the middle of each tread: half a step under the front of a tread, half a step over
				// its back, so the feet stay close to the steps that are drawn while walking stays smooth. Its foot is
				// a lip of half a step (the character steps up it); the top tread is flat.
				const float steps = (float)std::clamp(mesh.Steps, 1u, 64u);
				const float depth = size.y / steps;
				const float rise = h / steps;
				return MakeProfilePrism({ { y0, 0.0f }, { y0, 0.5f * rise }, { y1 - 0.5f * depth, h }, { y1, h }, { y1, 0.0f } }, x0, x1);
			}
			case MeshShape::Cylinder:
			{
				// MeshFactory's cylinder can be an oval (Size.x and Size.y differ): a ring of points at the base and the top.
				ColliderShape shape;
				shape.Type = ColliderShapeType::Prism;
				const uint32_t segments = 16;
				for (uint32_t ring = 0; ring < 2; ring++)
				{
					for (uint32_t i = 0; i < segments; i++)
					{
						const float angle = 6.28318530718f * (float)i / (float)segments;
						shape.Points.push_back({ std::cos(angle) * 0.5f * size.x, std::sin(angle) * 0.5f * size.y, ring == 0 ? 0.0f : h });
					}
				}
				return shape;
			}
			case MeshShape::Box:
			default:
				return MakeBox({ 0.0f, 0.0f, 0.5f * h }, 0.5f * size);
		}
	}

	bool ColliderShapes::HasColliders(entt::registry& registry, entt::entity entity)
	{
		return registry.has<BoxColliderComponent>(entity) || registry.has<SphereColliderComponent>(entity)
			|| registry.has<CapsuleColliderComponent>(entity) || registry.has<CylinderColliderComponent>(entity)
			|| (registry.has<MeshColliderComponent>(entity) && registry.has<MeshComponent>(entity)) || registry.has<TerrainComponent>(entity);
	}

	bool ColliderShapes::Get(entt::registry& registry, entt::entity entity, const glm::vec3& worldScale, std::vector<ColliderShape>& shapes)
	{
		if (registry.valid(entity) == false || registry.has<TransformComponent>(entity) == false)
		{
			return true;
		}

		const glm::vec3 scale = glm::abs(worldScale);
		const float largest = std::max(scale.x, std::max(scale.y, scale.z));
		const float largestXY = std::max(scale.x, scale.y);
		bool complete = true;

		if (registry.has<BoxColliderComponent>(entity))
		{
			const BoxColliderComponent& box = registry.get<BoxColliderComponent>(entity);
			ColliderShape shape = MakeBox(box.Offset * scale, 0.5f * glm::abs(box.Size) * scale);
			shape.Layer = box.Layer;
			shape.IsTrigger = box.IsTrigger;
			shapes.push_back(shape);
		}

		if (registry.has<SphereColliderComponent>(entity))
		{
			const SphereColliderComponent& sphere = registry.get<SphereColliderComponent>(entity);
			ColliderShape shape;
			shape.Type = ColliderShapeType::Sphere;
			shape.Centre = sphere.Offset * scale;
			shape.Radius = std::max(std::abs(sphere.Radius) * largest, MinHalfSize);
			shape.Layer = sphere.Layer;
			shape.IsTrigger = sphere.IsTrigger;
			shapes.push_back(shape);
		}

		if (registry.has<CapsuleColliderComponent>(entity))
		{
			const CapsuleColliderComponent& capsule = registry.get<CapsuleColliderComponent>(entity);
			ColliderShape shape;
			shape.Type = ColliderShapeType::Capsule;
			shape.Centre = capsule.Offset * scale;
			shape.Radius = std::max(std::abs(capsule.Radius) * largestXY, MinHalfSize);
			// A capsule shorter than its two half spheres is a sphere with a tiny straight part.
			shape.HalfHeight = std::max(0.5f * std::abs(capsule.Height) * scale.z - shape.Radius, MinHalfSize);
			shape.Layer = capsule.Layer;
			shape.IsTrigger = capsule.IsTrigger;
			shapes.push_back(shape);
		}

		if (registry.has<CylinderColliderComponent>(entity))
		{
			const CylinderColliderComponent& cylinder = registry.get<CylinderColliderComponent>(entity);
			ColliderShape shape;
			shape.Type = ColliderShapeType::Cylinder;
			shape.Centre = cylinder.Offset * scale;
			shape.Radius = std::max(std::abs(cylinder.Radius) * largestXY, MinHalfSize);
			shape.HalfHeight = std::max(0.5f * std::abs(cylinder.Height) * scale.z, MinHalfSize);
			shape.Layer = cylinder.Layer;
			shape.IsTrigger = cylinder.IsTrigger;
			shapes.push_back(shape);
		}

		if (registry.has<MeshColliderComponent>(entity) && registry.has<MeshComponent>(entity))
		{
			const MeshColliderComponent& collider = registry.get<MeshColliderComponent>(entity);
			const MeshComponent& mesh = registry.get<MeshComponent>(entity);
			if ((uint64_t)mesh.Model != 0)
			{
				// A model: its collision nodes (_COL, _WALK), else its bounding box.
				const Ref<Mesh> model = AssetManager::GetLoadedAsset<Mesh>(mesh.Model);
				if (model == nullptr)
				{
					complete = false;
				}
				else if (model->GetCollisionProxies().empty() == false)
				{
					for (const MeshCollisionProxy& proxy : model->GetCollisionProxies())
					{
						ColliderShape shape;
						shape.Type = proxy.Kind == MeshCollisionKind::Walk ? ColliderShapeType::TriangleMesh : ColliderShapeType::ConvexHull;
						shape.Points.reserve(proxy.Positions.size());
						for (const glm::vec3& position : proxy.Positions)
						{
							shape.Points.push_back(position * scale);
						}
						shape.Indices = proxy.Indices;
						shape.Layer = collider.Layer;
						shapes.push_back(std::move(shape));
					}
				}
				else
				{
					const glm::vec3 low = model->GetBoundsMin() * scale;
					const glm::vec3 high = model->GetBoundsMax() * scale;
					ColliderShape shape = MakeBox(0.5f * (low + high), 0.5f * glm::abs(high - low));
					shape.Layer = collider.Layer;
					shapes.push_back(shape);
				}
			}
			else
			{
				ColliderShape shape = MakeMeshShape(mesh, scale);
				shape.Layer = collider.Layer;
				shapes.push_back(shape);
			}
		}

		if (registry.has<TerrainComponent>(entity))
		{
			// The terrain's tops, ramps and walls as one hollow mesh, in its own space (terrains are unscaled, v1).
			const TerrainComponent& component = registry.get<TerrainComponent>(entity);
			const Ref<Terrain> terrain = (uint64_t)component.Terrain != 0 ? AssetManager::GetLoadedAsset<Terrain>(component.Terrain) : nullptr;
			if (terrain == nullptr)
			{
				complete = (uint64_t)component.Terrain == 0;
			}
			else
			{
				const TerrainCollisionMesh& collision = terrain->GetCollisionMesh();
				if (collision.Indices.empty() == false)
				{
					ColliderShape shape;
					shape.Type = ColliderShapeType::TriangleMesh;
					shape.Points = collision.Positions;
					shape.Indices = collision.Indices;
					shape.Layer = component.Layer;
					shapes.push_back(std::move(shape));
				}
			}
		}

		return complete;
	}
}
