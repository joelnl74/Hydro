#pragma once
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <string>

namespace Hydro
{
	// The YAML of every engine component, as scenes and prefabs save it: a Save and a Load per component, written in its
	// family's file (SceneComponentSerializers.cpp, ...) and registered with ComponentRegistry in RegisterEngineComponents.
	class ComponentSerializers
	{
	public:
		// Registers every engine component, in the order entities save them (ComponentRegistry calls it on first use).
		static void RegisterEngineComponents();

		// Pulls saved values the physics engine would choke on into range. max(minimum, value) also turns NaN into minimum.
		static float AtLeast(float minimum, float value);
		static const char* PhysicsLayerToString(PhysicsLayer layer);
		static PhysicsLayer PhysicsLayerFromString(const std::string& name, PhysicsLayer fallback);

	private:
		// SceneComponentSerializers.cpp
		static void SaveTransform(YAML::Emitter& out, const TransformComponent& transform);
		static void LoadTransform(const YAML::Node& node, TransformComponent& transform);
		static void SaveCamera(YAML::Emitter& out, const CameraComponent& cameraComponent);
		static void LoadCamera(const YAML::Node& node, CameraComponent& cameraComponent);
		static void SaveLight2D(YAML::Emitter& out, const Light2DComponent& light);
		static void LoadLight2D(const YAML::Node& node, Light2DComponent& light);
		static void SaveDirectionalLight(YAML::Emitter& out, const DirectionalLightComponent& light);
		static void LoadDirectionalLight(const YAML::Node& node, DirectionalLightComponent& light);

		// SpriteComponentSerializers.cpp
		static void SaveSpriteRenderer(YAML::Emitter& out, const SpriteRendererComponent& sprite);
		static void LoadSpriteRenderer(const YAML::Node& node, SpriteRendererComponent& sprite);
		static void SaveSpriteAnimator(YAML::Emitter& out, const SpriteAnimatorComponent& animator);
		static void LoadSpriteAnimator(const YAML::Node& node, SpriteAnimatorComponent& animator);
		static void SaveShadow2D(YAML::Emitter& out, const Shadow2DComponent& shadow);
		static void LoadShadow2D(const YAML::Node& node, Shadow2DComponent& shadow);
		static void SaveParticleEmitter2D(YAML::Emitter& out, const ParticleEmitter2DComponent& emitter);
		static void LoadParticleEmitter2D(const YAML::Node& node, ParticleEmitter2DComponent& emitter);

		// MeshComponentSerializers.cpp
		static void SaveMesh(YAML::Emitter& out, const MeshComponent& mesh);
		static void LoadMesh(const YAML::Node& node, MeshComponent& mesh);
		static void SaveTerrain(YAML::Emitter& out, const TerrainComponent& terrain);
		static void LoadTerrain(const YAML::Node& node, TerrainComponent& terrain);

		// PhysicsComponentSerializers.cpp
		static void SaveRigidbody2D(YAML::Emitter& out, const Rigidbody2DComponent& body);
		static void LoadRigidbody2D(const YAML::Node& node, Rigidbody2DComponent& body);
		static void SaveBoxCollider2D(YAML::Emitter& out, const BoxCollider2DComponent& collider);
		static void LoadBoxCollider2D(const YAML::Node& node, BoxCollider2DComponent& collider);
		static void SaveCircleCollider2D(YAML::Emitter& out, const CircleCollider2DComponent& collider);
		static void LoadCircleCollider2D(const YAML::Node& node, CircleCollider2DComponent& collider);
		static void SaveBoxCollider(YAML::Emitter& out, const BoxColliderComponent& collider);
		static void LoadBoxCollider(const YAML::Node& node, BoxColliderComponent& collider);
		static void SaveSphereCollider(YAML::Emitter& out, const SphereColliderComponent& collider);
		static void LoadSphereCollider(const YAML::Node& node, SphereColliderComponent& collider);
		static void SaveCapsuleCollider(YAML::Emitter& out, const CapsuleColliderComponent& collider);
		static void LoadCapsuleCollider(const YAML::Node& node, CapsuleColliderComponent& collider);
		static void SaveCylinderCollider(YAML::Emitter& out, const CylinderColliderComponent& collider);
		static void LoadCylinderCollider(const YAML::Node& node, CylinderColliderComponent& collider);
		static void SaveMeshCollider(YAML::Emitter& out, const MeshColliderComponent& collider);
		static void LoadMeshCollider(const YAML::Node& node, MeshColliderComponent& collider);
		static void SaveCharacterBody(YAML::Emitter& out, const CharacterBodyComponent& body);
		static void LoadCharacterBody(const YAML::Node& node, CharacterBodyComponent& body);

		// UIComponentSerializers.cpp
		static void SaveUICanvas(YAML::Emitter& out, const UICanvasComponent& canvas);
		static void LoadUICanvas(const YAML::Node& node, UICanvasComponent& canvas);
		static void SaveUIRect(YAML::Emitter& out, const UIRectComponent& rect);
		static void LoadUIRect(const YAML::Node& node, UIRectComponent& rect);
		static void SaveUIImage(YAML::Emitter& out, const UIImageComponent& image);
		static void LoadUIImage(const YAML::Node& node, UIImageComponent& image);
		static void SaveUIText(YAML::Emitter& out, const UITextComponent& text);
		static void LoadUIText(const YAML::Node& node, UITextComponent& text);
		static void SaveUILayoutGroup(YAML::Emitter& out, const UILayoutGroupComponent& group);
		static void LoadUILayoutGroup(const YAML::Node& node, UILayoutGroupComponent& group);
		static void SaveUIScrollView(YAML::Emitter& out, const UIScrollViewComponent& scroll);
		static void LoadUIScrollView(const YAML::Node& node, UIScrollViewComponent& scroll);
		static void SaveUIFocusVisibility(YAML::Emitter& out, const UIFocusVisibilityComponent& visibility);
		static void LoadUIFocusVisibility(const YAML::Node& node, UIFocusVisibilityComponent& visibility);
		static void SaveUIButton(YAML::Emitter& out, const UIButtonComponent& button);
		static void LoadUIButton(const YAML::Node& node, UIButtonComponent& button);
	};
}
