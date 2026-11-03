#pragma once

#include "Hydro/Core/UUID.h"

#include <cstdint>

namespace Hydro
{
	class Entity;
	class Scene;

	// A reference to an entity, kept as its UUID (a script property, saved and followed by copies). Get() looks it up
	// each call, so it is never stale. For a component or script on it, use ComponentRef<T>.
	class EntityRef
	{
	public:
		EntityRef() = default;
		explicit EntityRef(UUID id, Scene* scene = nullptr)
			: m_Id(id), m_Scene(scene)
		{
		}
		// The entity's UUID and scene; an empty Entity gives an empty reference.
		EntityRef(const Entity& entity);

		// Empty (false) when the reference is empty, has no scene, or the entity is gone. Pending destruction still
		// returns it: check Entity::IsValid.
		Entity Get() const;

		// The saved UUID (0 = none).
		UUID GetId() const { return m_Id; }
		// Set, whether or not its entity still exists.
		bool IsSet() const { return (uint64_t)m_Id != 0; }

		Scene* GetScene() const { return m_Scene; }
		// The scene Get() looks in: a script property's Apply sets its entity's scene.
		void SetScene(Scene* scene) { m_Scene = scene; }

		// Same target (the UUID; the scene isn't compared).
		bool operator==(const EntityRef& other) const { return (uint64_t)m_Id == (uint64_t)other.m_Id; }
		bool operator!=(const EntityRef& other) const { return (*this == other) == false; }

	private:
		UUID m_Id = UUID(0);
		Scene* m_Scene = nullptr;
	};
}
