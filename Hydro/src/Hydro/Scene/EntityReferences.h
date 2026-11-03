#pragma once
#include "Hydro/Core/UUID.h"
#include "Hydro/Scene/Entity.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Hydro
{
	class Scene;

	// What holds a reference from one entity to another.
	enum class EntityReferenceKind : uint8_t
	{
		Parent,        // RelationshipComponent::Parent: the hierarchy, not a use
		Component,     // an engine component's field (a button's neighbours, a scroll view's scrollbar)
		GameComponent, // an EntityRef in an HY_COMPONENT's fields
		Script         // a script's EntityRef / ComponentRef<T> property, or one inside a list or struct property
	};

	// One reference: who holds it and where ("NpcRouteScript" and "Route"; the field is null when it isn't named).
	struct EntityReference
	{
		Entity Source;
		EntityReferenceKind Kind = EntityReferenceKind::Component;
		const char* Holder = "";
		const char* Field = nullptr;
	};

	// Finds the references between a scene's entities: the one place that knows which fields hold an entity's UUID.
	// Copies remap them, and the inspector lists who uses an entity.
	class EntityReferences
	{
	public:
		// `visit(id, reference)` for every reference `entity` holds; it may change the UUID. Script references are the saved
		// values, which a running instance is set from.
		using Visitor = std::function<void(UUID& id, const EntityReference& reference)>;
		static void ForEach(Entity entity, const Visitor& visit);

		// The references other entities hold to `target`, in hierarchy order. Children (Parent) aren't included.
		static std::vector<EntityReference> FindReferrers(Scene& scene, UUID target);
	};
}
