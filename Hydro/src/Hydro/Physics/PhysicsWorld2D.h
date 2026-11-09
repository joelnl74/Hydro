#pragma once
#include "Hydro/Core/Base.h"

#include <glm/glm.hpp>
#include <entt.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Hydro
{
	class Scene;
	class Entity;
	struct SystemContext;

	// The 2D physics world (Box2D) of one running Scene, made at Scene::OnRuntimeStart. Dynamic bodies own their
	// transform, kinematic ones follow it, static ones ignore it; see docs/SceneAndECS.md, Physics.
	class PhysicsWorld2D
	{
	public:
		// World units are metres; gravity points down -Y (GravityScale 0 for a top-down game).
		static constexpr int SubStepCount = 4;
		static constexpr int MaxStepsPerFrame = 4;
		static constexpr float Gravity = -9.81f;

		explicit PhysicsWorld2D(Scene& scene);
		~PhysicsWorld2D();
		PhysicsWorld2D(const PhysicsWorld2D&) = delete;
		PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;

		// This update's collision events (two solid shapes) and trigger events (a sensor A and a shape B), for
		// Scene to deliver to scripts. Rebuilt by every Step.
		struct Event
		{
			enum class Type : uint8_t { CollisionBegin, CollisionEnd, TriggerBegin, TriggerEnd };
			Type Kind = Type::CollisionBegin;
			entt::entity A{ entt::null };
			entt::entity B{ entt::null };
		};
		const std::vector<Event>& GetEvents() const { return m_Events; }

		// Moves kinematic bodies to their transforms, runs the fixed steps, writes dynamic bodies back.
		void Step(const SystemContext& context);

		void OnEntityDestroyed(entt::entity handle);

		bool HasBody(entt::entity handle) const;
		// Makes the entity's body now if it should have one (spawned this frame). True when it has a body.
		bool EnsureBody(entt::entity handle);
		size_t GetBodyCount() const;

		// Dynamic and kinematic bodies only. World units per second.
		bool SetLinearVelocity(entt::entity handle, const glm::vec2& velocity);
		bool GetLinearVelocity(entt::entity handle, glm::vec2& velocity);

	private:
		struct Data;
		// False when it has no body to make or already has one.
		bool CreateBody(entt::entity handle);
		void CreateNewBodies();
		// Box2D's contact and sensor events of the step that just ran, into m_Events.
		void CollectEvents();

		Scope<Data> m_Data;
		std::vector<Event> m_Events;
		Scene* m_Scene = nullptr;
	};

	// Game code's access to an entity's 2D body. Does nothing (returns false) when not playing or without a body.
	class Physics2D
	{
	public:
		static bool HasBody(Entity entity);
		static bool SetLinearVelocity(Entity entity, const glm::vec2& velocity);
		static bool GetLinearVelocity(Entity entity, glm::vec2& velocity);
	};
}
