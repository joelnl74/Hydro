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

	// The 2D physics world of one running Scene (Box2D). Scene creates it when the game starts
	// (Scene::OnRuntimeStart) and destroys it when it stops, so a world only exists while playing and
	// the editor scene never has one.
	//
	// Which entities become bodies (built once, when the world is created): every entity with a
	// TransformComponent and a Rigidbody2DComponent, BoxCollider2DComponent or CircleCollider2DComponent.
	//  - No Rigidbody2D: a static body (it never moves).
	//  - Static / Dynamic / Kinematic as the Rigidbody2D says.
	// Entities that get such components after the start (spawned by a script, DuplicateEntity, a prefab)
	// get their body at the start of the next physics step, or at once when game code asks for it
	// (Physics2D::SetLinearVelocity / HasBody). The components are read when the body is made: changing
	// them later has no effect on the body.
	// Position (Translation.xy) and angle (Rotation.z) come from the TransformComponent when the world is
	// built; Z, and the sprite, are never touched.
	//
	// Who owns the transform while playing:
	//  - Dynamic: the simulation. After each update the transform is overwritten with the body's.
	//    To move a dynamic body, set its velocity (Physics2D::SetLinearVelocity); writing the transform
	//    has no effect on the body.
	//  - Kinematic: the game. Write its transform (a script, Bob, WaypointWalk); the body is carried to
	//    it with the right velocity, so it pushes dynamic bodies instead of teleporting through them.
	//  - Static: nobody. Moving the transform in play mode does not move the collider.
	//
	// The simulation runs in fixed steps of SystemContext::FixedDeltaTime (60 Hz): a frame runs as many
	// whole steps as its time allows (at most MaxStepsPerFrame, the rest of a long frame is dropped).
	// There is no interpolation between steps yet, so a frame that runs no step shows the last result.
	class PhysicsScene
	{
	public:
		// Box2D settings: world units are metres, gravity points down -Y (give a body GravityScale 0 for
		// a top-down game).
		static constexpr int SubStepCount = 4;
		static constexpr int MaxStepsPerFrame = 4;
		static constexpr float Gravity = -9.81f;

		explicit PhysicsScene(Scene& scene);
		~PhysicsScene();
		PhysicsScene(const PhysicsScene&) = delete;
		PhysicsScene& operator=(const PhysicsScene&) = delete;

		// What happened in this update's steps, for Scene to deliver to scripts. Collision events are
		// between two solid shapes starting / stopping to touch; trigger events are a sensor (A) and
		// another shape (B) starting / stopping to overlap. Rebuilt by every Step.
		struct Event
		{
			enum class Type : uint8_t { CollisionBegin, CollisionEnd, TriggerBegin, TriggerEnd };
			Type Kind = Type::CollisionBegin;
			entt::entity A{ entt::null };
			entt::entity B{ entt::null };
		};
		const std::vector<Event>& GetEvents() const { return m_Events; }

		// Physics stage of a runtime update: moves kinematic bodies to their transforms, runs the fixed
		// steps this frame's time allows, then writes the dynamic bodies' results to their transforms.
		void Step(const SystemContext& context);

		// Scene::DestroyEntity calls this so a destroyed entity's body goes with it.
		void OnEntityDestroyed(entt::entity handle);

		bool HasBody(entt::entity handle) const;
		// Makes the entity's body now if it has the components for one and none yet (an entity spawned this
		// frame). True when it has a body afterwards.
		bool EnsureBody(entt::entity handle);
		size_t GetBodyCount() const;

		// Dynamic and kinematic bodies only (false otherwise). World units per second.
		bool SetLinearVelocity(entt::entity handle, const glm::vec2& velocity);
		bool GetLinearVelocity(entt::entity handle, glm::vec2& velocity);

	private:
		struct Data;
		// Builds the body of one entity (false when it has none to make or already has it).
		bool CreateBody(entt::entity handle);
		// Builds bodies for entities that got physics components since the last step.
		void CreateNewBodies();
		// Reads Box2D's contact and sensor events of the step that just ran into m_Events.
		void CollectEvents();

		Scope<Data> m_Data;
		std::vector<Event> m_Events;
		Scene* m_Scene = nullptr;
	};

	// What game code uses: the entity's body in its scene's running world. All of them do nothing (and
	// return false) when the game isn't running or the entity has no body.
	namespace Physics2D
	{
		bool HasBody(Entity entity);
		bool SetLinearVelocity(Entity entity, const glm::vec2& velocity);
		bool GetLinearVelocity(Entity entity, glm::vec2& velocity);
	}
}
