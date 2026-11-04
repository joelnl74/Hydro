#include "hypch.h"
#include "JoltShapes.h"

#include "Hydro/Platform/Jolt/JoltMath.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>

namespace Hydro
{
	float JoltShapes::ConvexRadiusFor(float smallestHalfSize)
	{
		return std::min(JPH::cDefaultConvexRadius, 0.5f * smallestHalfSize);
	}

	JPH::ShapeRefC JoltShapes::Make(const std::vector<ColliderShape>& shapes, bool triggers, const char* entityName)
	{
		JPH::StaticCompoundShapeSettings compound;
		uint32_t count = 0;
		for (const ColliderShape& shape : shapes)
		{
			if (shape.IsTrigger != triggers)
			{
				continue;
			}

			JPH::Quat rotation = JPH::Quat::sIdentity();
			JPH::Vec3 position = JoltMath::ToJolt(shape.Centre);
			JPH::Ref<JPH::Shape> part;
			switch (shape.Type)
			{
				case ColliderShapeType::Box:
				{
					const float smallest = std::min(shape.HalfExtents.x, std::min(shape.HalfExtents.y, shape.HalfExtents.z));
					part = new JPH::BoxShape(JoltMath::ToJolt(shape.HalfExtents), ConvexRadiusFor(smallest));
					break;
				}
				case ColliderShapeType::Sphere:
					part = new JPH::SphereShape(shape.Radius);
					break;
				case ColliderShapeType::Capsule:
					part = new JPH::CapsuleShape(shape.HalfHeight, shape.Radius);
					rotation = JoltMath::StandUp();
					break;
				case ColliderShapeType::Cylinder:
					part = new JPH::CylinderShape(shape.HalfHeight, shape.Radius,
						ConvexRadiusFor(std::min(shape.HalfHeight, shape.Radius)));
					rotation = JoltMath::StandUp();
					break;
				case ColliderShapeType::TriangleMesh:
				{
					// Walkable ground as modelled (a bridge's arch, a slope): hollow triangles, allowed for static and
					// game-moved bodies only.
					JPH::VertexList vertices;
					vertices.reserve(shape.Points.size());
					for (const glm::vec3& point : shape.Points)
					{
						vertices.push_back(JPH::Float3(point.x, point.y, point.z));
					}
					JPH::IndexedTriangleList triangles;
					triangles.reserve(shape.Indices.size() / 3);
					for (size_t i = 0; i + 2 < shape.Indices.size(); i += 3)
					{
						triangles.push_back(JPH::IndexedTriangle(shape.Indices[i], shape.Indices[i + 1], shape.Indices[i + 2]));
					}
					position = JPH::Vec3::sZero();
					const JPH::ShapeSettings::ShapeResult result =
						JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
					if (result.HasError())
					{
						HY_CORE_WARN("Physics 3D: '{}': a walk mesh could not be built ({})", entityName, result.GetError().c_str());
						continue;
					}
					part = result.Get();
					break;
				}
				case ColliderShapeType::Prism:
				case ColliderShapeType::ConvexHull:
				{
					JPH::Array<JPH::Vec3> points;
					for (const glm::vec3& point : shape.Points)
					{
						points.push_back(JoltMath::ToJolt(point));
					}
					// The points are where the shape is: no extra offset. A sharp hull (no rounding) keeps ramps exact.
					position = JPH::Vec3::sZero();
					const JPH::ShapeSettings::ShapeResult result = JPH::ConvexHullShapeSettings(points, 0.0f).Create();
					if (result.HasError())
					{
						HY_CORE_WARN("Physics 3D: '{}': a collision shape could not be built ({})", entityName, result.GetError().c_str());
						continue;
					}
					part = result.Get();
					break;
				}
			}

			compound.AddShape(position, rotation, part);
			count++;
		}

		if (count == 0)
		{
			return nullptr;
		}

		const JPH::ShapeSettings::ShapeResult result = compound.Create();
		if (result.HasError())
		{
			HY_CORE_WARN("Physics 3D: '{}': its collision shapes could not be combined ({})", entityName, result.GetError().c_str());
			return nullptr;
		}
		return result.Get();
	}
}
