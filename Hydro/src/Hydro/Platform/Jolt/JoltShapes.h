#pragma once

#include "Hydro/Scene/ColliderShapes.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <vector>

// Hydro's collider shapes as one Jolt shape.

namespace Hydro
{
	class JoltShapes
	{
	public:
		// The Jolt shape of the shapes that are (or aren't) triggers, or null when there are none. `entityName` is
		// for the warnings about shapes that can't be built.
		static JPH::ShapeRefC Make(const std::vector<ColliderShape>& shapes, bool triggers, const char* entityName);

		// A convex radius rounds a shape's edges; it must fit inside the shape.
		static float ConvexRadiusFor(float smallestHalfSize);
	};
}
