#include <hypch.h>
#include "EntityReferences.h"

#include "ComponentRegistry.h"
#include "EntityRef.h"
#include "Scene.h"
#include "Scripting/ScriptableEntity.h"
#include "Hydro/Data/DataType.h"

namespace Hydro
{
	void EntityReferences::ForEach(Entity entity, const Visitor& visit)
	{
		EntityReference reference;
		reference.Source = entity;

		if (RelationshipComponent* relationship = entity.TryGetComponent<RelationshipComponent>())
		{
			reference.Kind = EntityReferenceKind::Parent;
			reference.Holder = "Relationship";
			reference.Field = "Parent";
			visit(relationship->Parent, reference);
		}

		reference.Kind = EntityReferenceKind::Component;
		if (UIButtonComponent* button = entity.TryGetComponent<UIButtonComponent>())
		{
			reference.Holder = "UI Button";
			reference.Field = "Up";
			visit(button->Up, reference);
			reference.Field = "Down";
			visit(button->Down, reference);
			reference.Field = "Left";
			visit(button->Left, reference);
			reference.Field = "Right";
			visit(button->Right, reference);
		}
		if (UIScrollViewComponent* scroll = entity.TryGetComponent<UIScrollViewComponent>())
		{
			reference.Holder = "UI Scroll View";
			reference.Field = "Scrollbar";
			visit(scroll->Scrollbar, reference);
		}

		reference.Kind = EntityReferenceKind::GameComponent;
		reference.Field = nullptr;
		for (const ComponentTypeInfo& type : ComponentRegistry::GetTypes())
		{
			void* data = type.Get(entity);
			if (data == nullptr)
			{
				continue;
			}
			reference.Holder = type.DisplayName.c_str();
			ForEachEntityRef(type.Type, data, [&](EntityRef& inner)
			{
				UUID id = inner.GetId();
				visit(id, reference);
				if (id != inner.GetId())
				{
					inner = EntityRef(id);
				}
			});
		}

		NativeScriptComponent* scripts = entity.TryGetComponent<NativeScriptComponent>();
		if (scripts == nullptr)
		{
			return;
		}
		reference.Kind = EntityReferenceKind::Script;
		for (NativeScriptComponent::Script& script : scripts->Scripts)
		{
			reference.Holder = script.Name.c_str();
			for (auto& property : script.Properties)
			{
				reference.Field = property.first.c_str();
				if (EntityRef* inner = std::get_if<EntityRef>(&property.second))
				{
					UUID id = inner->GetId();
					visit(id, reference);
					if (id != inner->GetId())
					{
						*inner = EntityRef(id);
					}
				}
				// The references inside a list or struct (a route's waypoints).
				else if (ScriptDataValue* data = std::get_if<ScriptDataValue>(&property.second); data != nullptr && data->IsSet())
				{
					ForEachEntityRef(*data->GetType(), data->Get(), [&](EntityRef& element)
					{
						UUID id = element.GetId();
						visit(id, reference);
						if (id != element.GetId())
						{
							element = EntityRef(id);
						}
					});
				}
			}
		}
	}

	std::vector<EntityReference> EntityReferences::FindReferrers(Scene& scene, UUID target)
	{
		std::vector<EntityReference> referrers;
		if ((uint64_t)target == 0)
		{
			return referrers;
		}
		for (const entt::entity handle : scene.GetEntityOrder())
		{
			ForEach(Entity{ handle, &scene }, [&](UUID& id, const EntityReference& reference)
			{
				if (id == target && reference.Kind != EntityReferenceKind::Parent)
				{
					referrers.push_back(reference);
				}
			});
		}
		return referrers;
	}
}
