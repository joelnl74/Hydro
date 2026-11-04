#include "hypch.h"
#include "Box2DShapes.h"

#include "Hydro/Scene/Components/Components.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	void Box2DShapes::Create(b2BodyId body, b2BodyType bodyType, entt::registry& registry, entt::entity handle, const glm::vec2& scale)
	{
		const float scaleX = std::abs(scale.x);
		const float scaleY = std::abs(scale.y);

		const auto makeShapeDef = [bodyType](const PhysicsMaterial2D& material, bool isSensor)
		{
			b2ShapeDef shapeDef = b2DefaultShapeDef();
			shapeDef.density = material.Density;
			shapeDef.material.friction = material.Friction;
			shapeDef.material.restitution = material.Restitution;
			shapeDef.isSensor = isSensor;
			// Box2D has both kinds of events off by default. Sensors need them to work; a shape is seen by sensors when
			// it can move (a static wall never triggers a zone).
			shapeDef.enableSensorEvents = isSensor || bodyType != b2_staticBody;
			shapeDef.enableContactEvents = isSensor == false;
			return shapeDef;
		};

		if (registry.has<BoxCollider2DComponent>(handle))
		{
			const BoxCollider2DComponent& box = registry.get<BoxCollider2DComponent>(handle);
			const float halfWidth = std::max(0.5f * std::abs(box.Size.x) * scaleX, MinHalfExtent);
			const float halfHeight = std::max(0.5f * std::abs(box.Size.y) * scaleY, MinHalfExtent);
			const b2Vec2 centre{ box.Offset.x * scaleX, box.Offset.y * scaleY };

			const b2Polygon polygon = b2MakeOffsetBox(halfWidth, halfHeight, centre, b2MakeRot(0.0f));
			const b2ShapeDef shapeDef = makeShapeDef(box.Material, box.IsSensor);
			b2CreatePolygonShape(body, &shapeDef, &polygon);
		}

		if (registry.has<CircleCollider2DComponent>(handle))
		{
			const CircleCollider2DComponent& circle = registry.get<CircleCollider2DComponent>(handle);

			b2Circle shape;
			shape.center = { circle.Offset.x * scaleX, circle.Offset.y * scaleY };
			shape.radius = std::max(std::abs(circle.Radius) * std::max(scaleX, scaleY), MinHalfExtent);
			const b2ShapeDef shapeDef = makeShapeDef(circle.Material, circle.IsSensor);
			b2CreateCircleShape(body, &shapeDef, &shape);
		}
	}
}
