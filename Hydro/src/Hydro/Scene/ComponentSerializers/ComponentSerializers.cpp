#include "hypch.h"
#include "ComponentSerializers.h"
#include "Hydro/Scene/ComponentRegistry.h"

#include <algorithm>

namespace Hydro
{
	void ComponentSerializers::RegisterEngineComponents()
	{
		// The parent: copied like the others, but EntitySerializer writes and reads it (scenes keep it, prefab roots don't).
		ComponentRegistry::RegisterEngineCopyOnly<RelationshipComponent>("RelationshipComponent");
		// In the order entities save them, so saved files keep their key order.
		ComponentRegistry::RegisterEngine<TransformComponent, &SaveTransform, &LoadTransform>("TransformComponent");
		ComponentRegistry::RegisterEngine<SpriteRendererComponent, &SaveSpriteRenderer, &LoadSpriteRenderer>("SpriteRendererComponent");
		ComponentRegistry::RegisterEngine<SpriteAnimatorComponent, &SaveSpriteAnimator, &LoadSpriteAnimator>("SpriteAnimatorComponent");
		ComponentRegistry::RegisterEngine<CameraComponent, &SaveCamera, &LoadCamera>("CameraComponent");
		ComponentRegistry::RegisterEngine<Light2DComponent, &SaveLight2D, &LoadLight2D>("Light2DComponent");
		ComponentRegistry::RegisterEngine<DirectionalLightComponent, &SaveDirectionalLight, &LoadDirectionalLight>(
			"DirectionalLightComponent");
		ComponentRegistry::RegisterEngine<MeshComponent, &SaveMesh, &LoadMesh>("MeshComponent");
		ComponentRegistry::RegisterEngine<Shadow2DComponent, &SaveShadow2D, &LoadShadow2D>("Shadow2DComponent");
		ComponentRegistry::RegisterEngine<Rigidbody2DComponent, &SaveRigidbody2D, &LoadRigidbody2D>("Rigidbody2DComponent");
		ComponentRegistry::RegisterEngine<BoxCollider2DComponent, &SaveBoxCollider2D, &LoadBoxCollider2D>("BoxCollider2DComponent");
		ComponentRegistry::RegisterEngine<CircleCollider2DComponent, &SaveCircleCollider2D, &LoadCircleCollider2D>(
			"CircleCollider2DComponent");
		ComponentRegistry::RegisterEngine<BoxColliderComponent, &SaveBoxCollider, &LoadBoxCollider>("BoxColliderComponent");
		ComponentRegistry::RegisterEngine<SphereColliderComponent, &SaveSphereCollider, &LoadSphereCollider>("SphereColliderComponent");
		ComponentRegistry::RegisterEngine<CapsuleColliderComponent, &SaveCapsuleCollider, &LoadCapsuleCollider>("CapsuleColliderComponent");
		ComponentRegistry::RegisterEngine<CylinderColliderComponent, &SaveCylinderCollider, &LoadCylinderCollider>(
			"CylinderColliderComponent");
		ComponentRegistry::RegisterEngine<MeshColliderComponent, &SaveMeshCollider, &LoadMeshCollider>("MeshColliderComponent");
		ComponentRegistry::RegisterEngine<CharacterBodyComponent, &SaveCharacterBody, &LoadCharacterBody>("CharacterBodyComponent");
		ComponentRegistry::RegisterEngine<TerrainComponent, &SaveTerrain, &LoadTerrain>("TerrainComponent");
		ComponentRegistry::RegisterEngine<UICanvasComponent, &SaveUICanvas, &LoadUICanvas>("UICanvasComponent");
		ComponentRegistry::RegisterEngine<UIRectComponent, &SaveUIRect, &LoadUIRect>("UIRectComponent");
		ComponentRegistry::RegisterEngine<UIImageComponent, &SaveUIImage, &LoadUIImage>("UIImageComponent");
		ComponentRegistry::RegisterEngine<UITextComponent, &SaveUIText, &LoadUIText>("UITextComponent");
		ComponentRegistry::RegisterEngine<UILayoutGroupComponent, &SaveUILayoutGroup, &LoadUILayoutGroup>("UILayoutGroupComponent");
		ComponentRegistry::RegisterEngine<UIScrollViewComponent, &SaveUIScrollView, &LoadUIScrollView>("UIScrollViewComponent");
		ComponentRegistry::RegisterEngine<UIFocusVisibilityComponent, &SaveUIFocusVisibility, &LoadUIFocusVisibility>(
			"UIFocusVisibilityComponent");
		ComponentRegistry::RegisterEngine<UIButtonComponent, &SaveUIButton, &LoadUIButton>("UIButtonComponent");
		ComponentRegistry::RegisterEngine<ParticleEmitter2DComponent, &SaveParticleEmitter2D, &LoadParticleEmitter2D>(
			"ParticleEmitter2DComponent");
	}

	float ComponentSerializers::AtLeast(float minimum, float value)
	{
		return std::max(minimum, value);
	}

	const char* ComponentSerializers::PhysicsLayerToString(PhysicsLayer layer)
	{
		switch (layer)
		{
		case PhysicsLayer::World: return "World";
		case PhysicsLayer::Player: return "Player";
		case PhysicsLayer::Npc: return "Npc";
		case PhysicsLayer::Trigger: return "Trigger";
		}

		return "World";
	}

	PhysicsLayer ComponentSerializers::PhysicsLayerFromString(const std::string& name, PhysicsLayer fallback)
	{
		if (name == "World")
		{
			return PhysicsLayer::World;
		}
		if (name == "Player")
		{
			return PhysicsLayer::Player;
		}
		if (name == "Npc")
		{
			return PhysicsLayer::Npc;
		}
		if (name == "Trigger")
		{
			return PhysicsLayer::Trigger;
		}

		return fallback;
	}
}
