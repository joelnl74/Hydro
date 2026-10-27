#pragma once
#include "Hydro/Core/Base.h"
#include "Hydro/Scene/Entity.h"

#include <glm/glm.hpp>
#include <entt.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Hydro
{
	struct SystemContext;

	// A ray's first hit in the 3D world (Physics3D::Raycast).
	struct RaycastHit3D
	{
		Entity HitEntity;
		glm::vec3 Point{ 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		float Distance = 0.0f; // from the ray's start, in metres
	};

	// What a character stands on (CharacterBody::GetGround).
	struct CharacterGround
	{
		bool IsGrounded = false; // standing on walkable ground (not too steep)
		Entity GroundEntity;     // the entity under it, when there is one
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		float SlopeDegrees = 0.0f;
	};

	// The 3D physics world of one running Scene (Jolt), the counterpart of PhysicsScene (2D, Box2D). Scene creates
	// it when the game starts (Scene::OnRuntimeStart) and destroys it when it stops; the editor scene never has one.
	// Jolt's types stay in PhysicsScene3D.cpp.
	//
	// Which entities are in it (World collision plan, Jolt physics MVP):
	//  - Every entity with a TransformComponent and 3D collider components (ColliderShapes) is a body: static, or
	//    kinematic when it has a NativeScriptComponent (the game moves it; the body follows its transform, so a
	//    walking villager blocks the player where it is). Colliders with IsTrigger make a second body of the entity
	//    on the Trigger layer, which blocks nothing.
	//  - Every entity with a TransformComponent and a CharacterBodyComponent is a character (Jolt's
	//    CharacterVirtual), standing on its position.
	// Entities that get such components later (spawned, duplicated, a prefab) get theirs at the start of the next
	// step; so does a model whose mesh wasn't loaded yet. The components are read when the body is made.
	//
	// Who owns the transform while playing:
	//  - Characters: the simulation. Game code sets the velocity it wants (SetCharacterVelocity); after each update
	//    the transform is the character's position blended between the last two fixed steps (interpolation), so
	//    it moves smoothly at any frame rate. Game code that writes the transform teleports the character there.
	//  - Kinematic bodies: the game. A changed transform moves the body at the next step.
	//  - Static bodies: nobody. Moving the transform in play mode does not move the collider.
	//
	// The simulation runs in fixed steps of SystemContext::FixedDeltaTime (60 Hz), at most MaxStepsPerFrame a frame,
	// in Z-up metres with gravity along -Z.
	class PhysicsScene3D
	{
	public:
		static constexpr int MaxStepsPerFrame = 4;
		static constexpr float Gravity = -9.81f;

		explicit PhysicsScene3D(Scene& scene);
		~PhysicsScene3D();
		PhysicsScene3D(const PhysicsScene3D&) = delete;
		PhysicsScene3D& operator=(const PhysicsScene3D&) = delete;

		// What happened in this update's steps, for Scene to deliver to scripts: a character (B) walking into or out
		// of a trigger (A). Rebuilt by every Step.
		struct Event
		{
			enum class Type : uint8_t { TriggerBegin, TriggerEnd };
			Type Kind = Type::TriggerBegin;
			entt::entity A{ entt::null };
			entt::entity B{ entt::null };
		};
		const std::vector<Event>& GetEvents() const { return m_Events; }

		// Physics stage of a runtime update: makes the bodies of new entities, moves kinematic bodies to their
		// transforms, runs the fixed steps this frame's time allows (characters move in each), then writes the
		// characters' blended positions to their transforms.
		void Step(const SystemContext& context);

		// Scene::DestroyEntity calls this so a destroyed entity's bodies and character go with it.
		void OnEntityDestroyed(entt::entity handle);

		bool HasBody(entt::entity handle) const;
		bool HasCharacter(entt::entity handle) const;
		size_t GetBodyCount() const;
		size_t GetCharacterCount() const;

		// The velocity the character wants, in metres per second along X and Y (Z is ignored: gravity and the
		// ground decide it). Kept until changed. False when the entity has no character.
		bool SetCharacterVelocity(entt::entity handle, const glm::vec3& velocity);
		bool GetCharacterGround(entt::entity handle, CharacterGround& ground) const;
		// The character's capsule (editor outlines): its feet (the simulated position, not the blended one), radius
		// and height. False when the entity has no character.
		bool GetCharacterCapsule(entt::entity handle, glm::vec3& feet, float& radius, float& height) const;

		// The first thing on the segment from `from` to `to` on one of the layers in `layerMask` (bit 1 << layer).
		bool Raycast(const glm::vec3& from, const glm::vec3& to, uint32_t layerMask, RaycastHit3D& hit) const;
		// Every entity with a body on one of the layers in `layerMask` that overlaps the sphere (no repeats).
		void OverlapSphere(const glm::vec3& centre, float radius, uint32_t layerMask, std::vector<Entity>& entities) const;

	private:
		struct Data;
		// Makes the entity's bodies and character when it should have them and doesn't (false: nothing to make yet).
		bool CreateBodies(entt::entity handle);
		void CreateNewBodies();
		void MoveKinematicBodies();
		void StepCharacters(float fixed);
		void CollectTriggerEvents();
		void WriteCharacterTransforms(float blend);

		Scope<Data> m_Data;
		std::vector<Event> m_Events;
		Scene* m_Scene = nullptr;
	};
}
