#include "hypch.h"
#include "Physics3D.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

namespace Hydro
{
	static PhysicsWorld3D* GetWorld(Entity& entity)
	{
		if (entity.IsValid() == false)
		{
			return nullptr;
		}
		Scene* scene = entity.GetScene();
		return scene != nullptr ? scene->GetPhysicsWorld3D() : nullptr;
	}

	bool Physics3D::Raycast(Scene& scene, const glm::vec3& from, const glm::vec3& to, uint32_t layerMask, RaycastHit3D& hit)
	{
		const PhysicsWorld3D* world = scene.GetPhysicsWorld3D();
		return world != nullptr && world->Raycast(from, to, layerMask, hit);
	}

	void Physics3D::OverlapSphere(Scene& scene, const glm::vec3& centre, float radius, uint32_t layerMask, std::vector<Entity>& entities)
	{
		const PhysicsWorld3D* world = scene.GetPhysicsWorld3D();
		if (world != nullptr)
		{
			world->OverlapSphere(centre, radius, layerMask, entities);
		}
	}

	bool Physics3D::HasBody(Entity entity)
	{
		const PhysicsWorld3D* world = GetWorld(entity);
		return world != nullptr && world->HasBody((entt::entity)entity);
	}

	bool CharacterBody::Move(Entity entity, const glm::vec3& velocity)
	{
		PhysicsWorld3D* world = GetWorld(entity);
		return world != nullptr && world->SetCharacterVelocity((entt::entity)entity, velocity);
	}

	bool CharacterBody::GetGround(Entity entity, CharacterGround& ground)
	{
		const PhysicsWorld3D* world = GetWorld(entity);
		return world != nullptr && world->GetCharacterGround((entt::entity)entity, ground);
	}

	bool CharacterBody::IsCharacter(Entity entity)
	{
		const PhysicsWorld3D* world = GetWorld(entity);
		return world != nullptr && world->HasCharacter((entt::entity)entity);
	}
}
