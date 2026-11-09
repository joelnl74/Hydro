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

	// A ray's first hit in the 3D world.
	struct RaycastHit3D
	{
		Entity HitEntity;
		glm::vec3 Point{ 0.0f };
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		float Distance = 0.0f; // from the ray's start, in metres
	};

	struct CharacterGround
	{
		bool IsGrounded = false; // standing on walkable ground (not too steep)
		Entity GroundEntity;     // the entity under it, when there is one
		glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
		float SlopeDegrees = 0.0f;
	};

	// The 3D physics world (Jolt) of one running Scene, made at Scene::OnRuntimeStart; Jolt types stay in the .cpp.
	// Characters own their transform (interpolated), kinematic bodies follow it; see docs/SceneAndECS.md, Physics.
	class PhysicsWorld3D
	{
	public:
		static constexpr int MaxStepsPerFrame = 4;
		static constexpr float Gravity = -9.81f;

		explicit PhysicsWorld3D(Scene& scene);
		~PhysicsWorld3D();
		PhysicsWorld3D(const PhysicsWorld3D&) = delete;
		PhysicsWorld3D& operator=(const PhysicsWorld3D&) = delete;

		// This update's trigger events (a character B entering or leaving a trigger A). Rebuilt by every Step.
		struct Event
		{
			enum class Type : uint8_t { TriggerBegin, TriggerEnd };
			Type Kind = Type::TriggerBegin;
			entt::entity A{ entt::null };
			entt::entity B{ entt::null };
		};
		const std::vector<Event>& GetEvents() const { return m_Events; }

		// Makes new bodies, moves kinematic bodies, runs the fixed steps, writes characters' blended positions back.
		void Step(const SystemContext& context);

		// Returns the triggers a destroyed character was inside, so the scene can send OnTriggerExit (#150).
		std::vector<entt::entity> OnEntityDestroyed(entt::entity handle);

		bool HasBody(entt::entity handle) const;
		bool HasCharacter(entt::entity handle) const;
		size_t GetBodyCount() const;
		size_t GetCharacterCount() const;

		// Metres per second along X and Y (gravity and the ground decide Z). Kept until changed.
		bool SetCharacterVelocity(entt::entity handle, const glm::vec3& velocity);
		bool GetCharacterGround(entt::entity handle, CharacterGround& ground) const;
		// Feet at the simulated position (not the blended one). False when the entity has no character.
		bool GetCharacterCapsule(entt::entity handle, glm::vec3& feet, float& radius, float& height) const;

		// The first hit on the segment, on the layers in `layerMask` (bit 1 << layer).
		bool Raycast(const glm::vec3& from, const glm::vec3& to, uint32_t layerMask, RaycastHit3D& hit) const;
		// Entities with a body on those layers overlapping the sphere, without repeats.
		void OverlapSphere(const glm::vec3& centre, float radius, uint32_t layerMask, std::vector<Entity>& entities) const;

	private:
		struct Data;
		// False when there is nothing to make yet.
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
