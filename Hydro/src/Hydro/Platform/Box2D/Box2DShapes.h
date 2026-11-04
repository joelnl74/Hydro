#pragma once

#include <box2d/box2d.h>
#include <entt.hpp>
#include <glm/glm.hpp>

// Hydro's 2D collider components as Box2D shapes.

namespace Hydro
{
	class Box2DShapes
	{
	public:
		// Box2D asserts on shapes thinner than its linear slop (0.005), so colliders are never smaller.
		static constexpr float MinHalfExtent = 0.01f;

		// Adds the entity's BoxCollider2D and CircleCollider2D as shapes of the body, at its world scale.
		static void Create(b2BodyId body, b2BodyType bodyType, entt::registry& registry, entt::entity handle, const glm::vec2& scale);
	};
}
