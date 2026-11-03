#include "ComponentDrawers.h"
#include "InspectorUI.h"

#include <Hydro/Scene/Scripting/ScriptableEntity.h>

namespace Hydro
{
	// The rows both 2D colliders share: how the surface behaves, and whether it is only a trigger.
	static void DrawColliderRows(PhysicsMaterial2D& material, bool& isSensor)
	{
		InspectorUI::Row("Density", [&]() { InspectorUI::InputFloat("##Density", material.Density, "%.2f", 0.0f, 100000.0f); });
		InspectorUI::Row("Friction", [&]() { InspectorUI::InputFloat("##Friction", material.Friction, "%.2f", 0.0f, 100.0f); });
		InspectorUI::Row("Restitution", [&]() { InspectorUI::InputFloat("##Restitution", material.Restitution, "%.2f", 0.0f, 1.0f); });
		InspectorUI::Row("Is Sensor", [&]()
		{
			ImGui::Checkbox("##IsSensor", &isSensor);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A sensor detects overlaps (trigger events) but nothing bounces off it.");
			}
		});
	}

	// The rows every 3D collider shares: its layer, and whether it only detects characters walking in and out.
	static void DrawCollider3DRows(Entity entity, PhysicsLayer& layer, bool& isTrigger)
	{
		InspectorUI::PhysicsLayerRow(layer);
		InspectorUI::Row("Is Trigger", [&]()
		{
			ImGui::Checkbox("##IsTrigger", &isTrigger);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("A trigger blocks nothing: characters walking in and out send OnTriggerEnter / OnTriggerExit.");
			}
		});
		ImGui::TextDisabled(entity.HasComponent<NativeScriptComponent>() ? "Scripted: the collider follows the transform while playing."
			: "A static collider. Offset is its centre, before the transform's scale.");
	}

	static void DrawRigidbody2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<Rigidbody2DComponent>("Rigidbody 2D", entity, context, [&](Rigidbody2DComponent& body)
		{
			const char* typeNames[] = { "Static", "Dynamic", "Kinematic" };
			int type = (int)body.Type;
			InspectorUI::Row("Type", [&]()
			{
				if (ImGui::Combo("##BodyType", &type, typeNames, 3))
				{
					body.Type = (Rigidbody2DType)type;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Static: never moves.\nDynamic: moved by forces, gravity and collisions.\nKinematic: moved only by the game; pushes dynamic bodies.");
				}
			});
			InspectorUI::Row("Gravity Scale",
				[&]() { InspectorUI::InputFloat("##GravityScale", body.GravityScale, "%.2f", -100.0f, 100.0f); });
			InspectorUI::Row("Linear Damping",
				[&]() { InspectorUI::InputFloat("##LinearDamping", body.LinearDamping, "%.2f", 0.0f, 1000.0f); });
			InspectorUI::Row("Angular Damping",
				[&]() { InspectorUI::InputFloat("##AngularDamping", body.AngularDamping, "%.2f", 0.0f, 1000.0f); });
			InspectorUI::Row("Fixed Rotation", [&]() { ImGui::Checkbox("##FixedRotation", &body.FixedRotation); });
			InspectorUI::Row("Bullet", [&]()
			{
				ImGui::Checkbox("##IsBullet", &body.IsBullet);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Continuous collision for fast, small bodies such as projectiles. Costs more.");
				}
			});
		});
	}

	static void DrawBoxCollider2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<BoxCollider2DComponent>("Box Collider 2D", entity, context, [&](BoxCollider2DComponent& collider)
		{
			InspectorUI::Row("Offset X",
				[&]() { InspectorUI::InputFloat("##BoxOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Offset Y",
				[&]() { InspectorUI::InputFloat("##BoxOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Width", [&]() { InspectorUI::InputFloat("##BoxWidth", collider.Size.x, "%.2f", 0.001f, 10000.0f); });
			InspectorUI::Row("Height", [&]() { InspectorUI::InputFloat("##BoxHeight", collider.Size.y, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
			}
		});
	}

	static void DrawCircleCollider2D(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<CircleCollider2DComponent>("Circle Collider 2D", entity, context, [&](CircleCollider2DComponent& collider)
		{
			InspectorUI::Row("Offset X",
				[&]() { InspectorUI::InputFloat("##CircleOffsetX", collider.Offset.x, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Offset Y",
				[&]() { InspectorUI::InputFloat("##CircleOffsetY", collider.Offset.y, "%.2f", -10000.0f, 10000.0f); });
			InspectorUI::Row("Radius", [&]() { InspectorUI::InputFloat("##CircleRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawColliderRows(collider.Material, collider.IsSensor);
			if (entity.HasComponent<Rigidbody2DComponent>() == false)
			{
				ImGui::TextDisabled("No Rigidbody 2D: this is a static collider.");
			}
		});
	}

	static void DrawBoxCollider(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<BoxColliderComponent>("Box Collider", entity, context, [&](BoxColliderComponent& collider)
		{
			InspectorUI::DrawVec3("Offset", collider.Offset);
			InspectorUI::DrawVec3("Size", collider.Size, 1.0f);
			collider.Size = glm::max(collider.Size, glm::vec3(0.001f));
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});
	}

	static void DrawSphereCollider(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<SphereColliderComponent>("Sphere Collider", entity, context, [&](SphereColliderComponent& collider)
		{
			InspectorUI::DrawVec3("Offset", collider.Offset);
			InspectorUI::Row("Radius", [&]() { InspectorUI::InputFloat("##SphereRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});
	}

	static void DrawCapsuleCollider(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<CapsuleColliderComponent>("Capsule Collider", entity, context, [&](CapsuleColliderComponent& collider)
		{
			InspectorUI::DrawVec3("Offset", collider.Offset);
			InspectorUI::Row("Radius", [&]() { InspectorUI::InputFloat("##CapsuleRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			InspectorUI::Row("Height", [&]() { InspectorUI::InputFloat("##CapsuleHeight", collider.Height, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});
	}

	static void DrawCylinderCollider(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<CylinderColliderComponent>("Cylinder Collider", entity, context, [&](CylinderColliderComponent& collider)
		{
			InspectorUI::DrawVec3("Offset", collider.Offset);
			InspectorUI::Row("Radius", [&]() { InspectorUI::InputFloat("##CylinderRadius", collider.Radius, "%.2f", 0.001f, 10000.0f); });
			InspectorUI::Row("Height", [&]() { InspectorUI::InputFloat("##CylinderHeight", collider.Height, "%.2f", 0.001f, 10000.0f); });
			DrawCollider3DRows(entity, collider.Layer, collider.IsTrigger);
		});
	}

	static void DrawMeshCollider(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<MeshColliderComponent>("Mesh Collider", entity, context, [&](MeshColliderComponent& collider)
		{
			InspectorUI::PhysicsLayerRow(collider.Layer);
			if (entity.HasComponent<MeshComponent>() == false)
			{
				ImGui::TextDisabled("Needs a Mesh on the entity.");
			}
			else if ((uint64_t)entity.GetComponent<MeshComponent>().Model != 0)
			{
				ImGui::TextDisabled("A model collides as its bounding box for now.");
			}
			else
			{
				ImGui::TextDisabled("The built-in shape collides as itself (stairs as a ramp).");
			}
		});
	}

	static void DrawCharacterBody(Entity entity, InspectorContext& context)
	{
		ComponentDrawers::Section<CharacterBodyComponent>("Character Body", entity, context, [&](CharacterBodyComponent& body)
		{
			InspectorUI::Row("Radius", [&]() { InspectorUI::InputFloat("##CharacterRadius", body.Radius, "%.2f", 0.05f, 100.0f); });
			InspectorUI::Row("Height", [&]() { InspectorUI::InputFloat("##CharacterHeight", body.Height, "%.2f", 0.2f, 100.0f); });
			InspectorUI::Row("Max Slope",
				[&]() { InspectorUI::InputFloat("##CharacterMaxSlope", body.MaxSlopeDegrees, "%.1f", 0.0f, 89.0f); });
			InspectorUI::Row("Step Height",
				[&]() { InspectorUI::InputFloat("##CharacterStepHeight", body.StepHeight, "%.2f", 0.0f, 10.0f); });
			InspectorUI::Row("Stays On Ledges", [&]()
			{
				ImGui::Checkbox("##CharacterStaysOnLedges", &body.StaysOnLedges);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Never walks off a drop higher than Step Height: it stops at the edge instead of falling.");
				}
			});
			InspectorUI::PhysicsLayerRow(body.Layer);
			ImGui::TextDisabled("A capsule on the entity's feet; scripts move it with CharacterBody::Move.");
		});
	}

	void ComponentDrawers::AddPhysicsDrawers(std::vector<ComponentDrawer>& drawers)
	{
		drawers.push_back(Make<Rigidbody2DComponent>("Rigidbody 2D", &DrawRigidbody2D,
			&AddWithTransform<Rigidbody2DComponent>, "Physics 2D"));
		drawers.push_back(Make<BoxCollider2DComponent>("Box Collider 2D", &DrawBoxCollider2D,
			&AddWithTransform<BoxCollider2DComponent>, "Physics 2D"));
		drawers.push_back(Make<CircleCollider2DComponent>("Circle Collider 2D", &DrawCircleCollider2D,
			&AddWithTransform<CircleCollider2DComponent>, "Physics 2D"));
		drawers.push_back(Make<BoxColliderComponent>("Box Collider", &DrawBoxCollider,
			&AddWithTransform<BoxColliderComponent>, "Physics 3D"));
		drawers.push_back(Make<SphereColliderComponent>("Sphere Collider", &DrawSphereCollider,
			&AddWithTransform<SphereColliderComponent>, "Physics 3D"));
		drawers.push_back(Make<CapsuleColliderComponent>("Capsule Collider", &DrawCapsuleCollider,
			&AddWithTransform<CapsuleColliderComponent>, "Physics 3D"));
		drawers.push_back(Make<CylinderColliderComponent>("Cylinder Collider", &DrawCylinderCollider,
			&AddWithTransform<CylinderColliderComponent>, "Physics 3D"));
		// Only on an entity with a Mesh: it collides as that mesh.
		drawers.push_back(Make<MeshColliderComponent>("Mesh Collider", &DrawMeshCollider, &AddOnly<MeshColliderComponent>, "Physics 3D"));
		drawers.back().CanAdd = &Has<MeshComponent>;
		drawers.push_back(Make<CharacterBodyComponent>("Character Body", &DrawCharacterBody,
			&AddWithTransform<CharacterBodyComponent>, "Physics 3D"));
	}
}
