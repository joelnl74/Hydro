#include "hypch.h"
#include "PhysicsScene.h"

#include "Hydro/Core/Assert.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SystemContext.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	namespace
	{
		constexpr float Pi = 3.14159265358979f;
		// Box2D asserts on shapes thinner than its linear slop (0.005), so colliders are never smaller.
		constexpr float MinHalfExtent = 0.01f;

		float WrapAngle(float radians)
		{
			while (radians > Pi)
				radians -= 2.0f * Pi;
			while (radians < -Pi)
				radians += 2.0f * Pi;
			return radians;
		}
	}

	struct PhysicsScene::Data
	{
		struct Body
		{
			entt::entity Handle{ entt::null };
			b2BodyId Id = b2_nullBodyId;
			b2BodyType Type = b2_staticBody;
		};

		b2WorldId World = b2_nullWorldId;
		std::vector<Body> Bodies;
		std::unordered_map<entt::entity, size_t> Index; // entity -> position in Bodies
		float Accumulator = 0.0f;
	};

	// Adds the entity's collider components as shapes of the body.
	static void CreateShapes(b2BodyId body, entt::registry& registry, entt::entity handle, const TransformComponent& transform)
	{
		const float scaleX = std::abs(transform.Scale.x);
		const float scaleY = std::abs(transform.Scale.y);

		const auto makeShapeDef = [](const PhysicsMaterial2D& material, bool isSensor)
		{
			b2ShapeDef shapeDef = b2DefaultShapeDef();
			shapeDef.density = material.Density;
			shapeDef.material.friction = material.Friction;
			shapeDef.material.restitution = material.Restitution;
			shapeDef.isSensor = isSensor;
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

	PhysicsScene::PhysicsScene(Scene& scene)
		: m_Data(CreateScope<Data>()), m_Scene(&scene)
	{
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { 0.0f, Gravity };
		m_Data->World = b2CreateWorld(&worldDef);

		entt::registry& registry = scene.m_Registry;
		for (entt::entity handle : scene.GetEntityOrder())
		{
			if (registry.has<TransformComponent>(handle) == false)
			{
				continue;
			}

			const bool hasRigidbody = registry.has<Rigidbody2DComponent>(handle);
			if (hasRigidbody == false && registry.has<BoxCollider2DComponent>(handle) == false
				&& registry.has<CircleCollider2DComponent>(handle) == false)
			{
				continue;
			}

			const TransformComponent& transform = registry.get<TransformComponent>(handle);

			b2BodyDef bodyDef = b2DefaultBodyDef();
			bodyDef.type = b2_staticBody;
			if (hasRigidbody)
			{
				const Rigidbody2DComponent& rigidbody = registry.get<Rigidbody2DComponent>(handle);
				switch (rigidbody.Type)
				{
				case Rigidbody2DType::Dynamic: bodyDef.type = b2_dynamicBody; break;
				case Rigidbody2DType::Kinematic: bodyDef.type = b2_kinematicBody; break;
				default: bodyDef.type = b2_staticBody; break;
				}
				bodyDef.fixedRotation = rigidbody.FixedRotation;
				bodyDef.isBullet = rigidbody.IsBullet;
				bodyDef.gravityScale = rigidbody.GravityScale;
				bodyDef.linearDamping = rigidbody.LinearDamping;
				bodyDef.angularDamping = rigidbody.AngularDamping;
			}
			bodyDef.position = { transform.Translation.x, transform.Translation.y };
			bodyDef.rotation = b2MakeRot(transform.Rotation.z);
			// For the collision callbacks (E2): the entity this body belongs to.
			bodyDef.userData = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<uint32_t>(handle)));

			const b2BodyId body = b2CreateBody(m_Data->World, &bodyDef);
			CreateShapes(body, registry, handle, transform);

			Data::Body entry;
			entry.Handle = handle;
			entry.Id = body;
			entry.Type = bodyDef.type;
			m_Data->Index[handle] = m_Data->Bodies.size();
			m_Data->Bodies.push_back(entry);
		}
	}

	PhysicsScene::~PhysicsScene()
	{
		if (m_Data != nullptr && b2World_IsValid(m_Data->World))
		{
			b2DestroyWorld(m_Data->World);
		}
	}

	void PhysicsScene::Step(const SystemContext& context)
	{
		if (context.IsPlaying == false || context.FixedDeltaTime <= 0.0f)
		{
			return;
		}

		const float fixed = context.FixedDeltaTime;
		m_Data->Accumulator += context.DeltaTime;

		int steps = 0;
		while (m_Data->Accumulator >= fixed && steps < MaxStepsPerFrame)
		{
			m_Data->Accumulator -= fixed;
			steps++;
		}
		if (m_Data->Accumulator >= fixed)
		{
			// A long frame (a breakpoint, a hitch): drop the backlog instead of running behind forever.
			m_Data->Accumulator = 0.0f;
		}
		if (steps == 0)
		{
			return;
		}

		entt::registry& registry = m_Scene->m_Registry;
		const float span = fixed * static_cast<float>(steps);

		// Kinematic bodies: carry them to where the game put their transform during these steps.
		for (const Data::Body& body : m_Data->Bodies)
		{
			if (body.Type != b2_kinematicBody || registry.valid(body.Handle) == false || registry.has<TransformComponent>(body.Handle) == false)
			{
				continue;
			}

			const TransformComponent& transform = registry.get<TransformComponent>(body.Handle);
			const b2Vec2 position = b2Body_GetPosition(body.Id);
			const float angle = b2Rot_GetAngle(b2Body_GetRotation(body.Id));

			b2Body_SetLinearVelocity(body.Id, { (transform.Translation.x - position.x) / span, (transform.Translation.y - position.y) / span });
			b2Body_SetAngularVelocity(body.Id, WrapAngle(transform.Rotation.z - angle) / span);
		}

		for (int i = 0; i < steps; i++)
		{
			b2World_Step(m_Data->World, fixed, SubStepCount);
		}

		// Dynamic bodies: the simulation's result becomes the entity's transform. A body that is asleep
		// has not moved.
		for (const Data::Body& body : m_Data->Bodies)
		{
			if (body.Type != b2_dynamicBody || registry.valid(body.Handle) == false || registry.has<TransformComponent>(body.Handle) == false)
			{
				continue;
			}
			if (b2Body_IsAwake(body.Id) == false)
			{
				continue;
			}

			TransformComponent& transform = registry.get<TransformComponent>(body.Handle);
			const b2Vec2 position = b2Body_GetPosition(body.Id);
			transform.Translation.x = position.x;
			transform.Translation.y = position.y;
			transform.Rotation.z = b2Rot_GetAngle(b2Body_GetRotation(body.Id));
		}
	}

	void PhysicsScene::OnEntityDestroyed(entt::entity handle)
	{
		const auto found = m_Data->Index.find(handle);
		if (found == m_Data->Index.end())
		{
			return;
		}

		const size_t index = found->second;
		b2DestroyBody(m_Data->Bodies[index].Id);

		// Swap-remove, keeping the index map in step.
		const size_t last = m_Data->Bodies.size() - 1;
		if (index != last)
		{
			m_Data->Bodies[index] = m_Data->Bodies[last];
			m_Data->Index[m_Data->Bodies[index].Handle] = index;
		}
		m_Data->Bodies.pop_back();
		m_Data->Index.erase(handle);
	}

	bool PhysicsScene::HasBody(entt::entity handle) const
	{
		return m_Data->Index.find(handle) != m_Data->Index.end();
	}

	size_t PhysicsScene::GetBodyCount() const
	{
		return m_Data->Bodies.size();
	}

	bool PhysicsScene::SetLinearVelocity(entt::entity handle, const glm::vec2& velocity)
	{
		const auto found = m_Data->Index.find(handle);
		if (found == m_Data->Index.end() || m_Data->Bodies[found->second].Type == b2_staticBody)
		{
			return false;
		}

		b2Body_SetLinearVelocity(m_Data->Bodies[found->second].Id, { velocity.x, velocity.y });
		return true;
	}

	bool PhysicsScene::GetLinearVelocity(entt::entity handle, glm::vec2& velocity) const
	{
		const auto found = m_Data->Index.find(handle);
		if (found == m_Data->Index.end() || m_Data->Bodies[found->second].Type == b2_staticBody)
		{
			return false;
		}

		const b2Vec2 value = b2Body_GetLinearVelocity(m_Data->Bodies[found->second].Id);
		velocity = { value.x, value.y };
		return true;
	}

	namespace Physics2D
	{
		static PhysicsScene* GetWorld(Entity& entity)
		{
			if (entity.IsValid() == false)
			{
				return nullptr;
			}
			Scene* scene = entity.GetScene();
			return scene != nullptr ? scene->GetPhysicsScene() : nullptr;
		}

		bool HasBody(Entity entity)
		{
			PhysicsScene* world = GetWorld(entity);
			return world != nullptr && world->HasBody(static_cast<entt::entity>(entity));
		}

		bool SetLinearVelocity(Entity entity, const glm::vec2& velocity)
		{
			PhysicsScene* world = GetWorld(entity);
			return world != nullptr && world->SetLinearVelocity(static_cast<entt::entity>(entity), velocity);
		}

		bool GetLinearVelocity(Entity entity, glm::vec2& velocity)
		{
			PhysicsScene* world = GetWorld(entity);
			return world != nullptr && world->GetLinearVelocity(static_cast<entt::entity>(entity), velocity);
		}
	}
}
