#pragma once

#include "Hydro/Core/UUID.h"

#include <cstdint>

namespace Hydro
{
	class Entity;
	class Scene;

	// A reference to an entity, kept as its UUID (Scripting plan SC4): a script's HY_PROPERTY set in the editor (pick
	// an entity, or drag one from the hierarchy onto the field), saved as the UUID and followed by copies (a prefab
	// instance's or a duplicate's reference to an entity inside the copied set points at its own copy,
	// Scene::RemapEntityReferences). Get() looks the entity up each call in the scene it belongs to, so it is never
	// stale: an empty Entity once the target is gone. A script's references belong to its entity's scene (the
	// property's Apply sets it); one made in code from an Entity belongs to that entity's scene.
	//
	//     HY_PROPERTY(Required) EntityRef Door;
	//     ...
	//     if (Entity door = Door.Get()) { ... }
	//
	// For a component or a script on the entity, ComponentRef<T> (ComponentRef.h).
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

		// The entity, or an empty Entity (false) when the reference is empty, has no scene, or the entity is gone.
		// An entity pending destruction is still returned: check Entity::IsValid for that.
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
