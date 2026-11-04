#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <iostream>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <Hydro/Core/Base.h>
#include <Hydro/Core/UUID.h>
#include <Hydro/Renderer/SceneCamera.h>
#include <Hydro/Asset/Asset.h>
#include <Hydro/Renderer/Image.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/TextLayout.h>

#include <vector>
#include <glm/gtx/quaternion.hpp>

namespace Hydro
{
	class Entity;
	class Scene;
	class UUID;

	// How a sprite's quad turns towards the camera. The ground is the XY plane and +Z is up, so a top-down
	// camera (looking down -Z) sees every facing as a flat quad.
	enum class SpriteFacing : uint8_t
	{
		Flat = 0,  // lies on the ground (XY), as it is authored: ground, roads, decals, water, bridges
		Upright,   // stands on its bottom edge and turns to face the camera (yaw and pitch, never roll):
		           // trees, buildings, props, characters
		Billboard  // turns about its centre to face the camera fully: particles, glows, icons, world-space UI
	};

	// How a sprite draws in a view with shared depth (RenderFeatureSettings::Depth). Without shared depth every
	// sprite is translucent and drawn in sort order.
	enum class SpriteAlphaMode : uint8_t
	{
		Default = 0,  // the view's RenderFeatureSettings::DefaultSpriteAlpha (translucent in the 2D preset)
		Cutout,       // pixels under half alpha are dropped, the rest are opaque: depth test and write, no blending
		Translucent,  // alpha blended: depth test, no depth write, sorted back to front (glows, water, effects)
		Additive      // light added onto what is behind, unlit, sorted with the translucent ones;
		              // works without shared depth too (light shafts, glows)
	};

	// A quad drawn with the entity's TransformComponent: Texture (white when 0) times Color. A Multiple texture
	// (sprite sheet) draws sprite SpriteIndex, placed by its pivot; Facing decides how it turns to a tilted camera.
	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// A Texture2D asset (0 = none), saved as TextureHandle. Drawn only once loaded: scene loading and the editor's
		// pickers load it; game code calls AssetManager::GetAsset<Texture2D>(handle) once.
		AssetHandle Texture = 0;
		float TilingFactor = 1.0f; // Single textures only
		// Higher draws on top. Ties: Flat sprites first (lower z first), then Upright and Billboard ones far to near
		// along the camera's ground direction, then scene order.
		int32_t SortOrder = 0;
		// Texture2D::GetSprite index: 0 is the whole Single texture or the first sprite of a sheet; out of range draws 0.
		// A playing SpriteAnimatorComponent sets it every frame.
		uint32_t SpriteIndex = 0;
		bool FlipX = false;
		bool FlipY = false;
		SpriteFacing Facing = SpriteFacing::Flat;
		SpriteAlphaMode AlphaMode = SpriteAlphaMode::Default;
		// HUD: drawn in the overlay pass after everything else and the lighting, unlit, never depth tested.
		bool Overlay = false;
		// A cut-out Upright or Billboard sprite casts a sun shadow (RenderFeatureSettings::SpriteShadows); turn off for
		// grass, flowers and anything that shouldn't darken the ground.
		bool CastShadows = true;
		// Texture times Color times EmissionColor times EmissionIntensity is added after the lighting, so flames and
		// lit windows glow in the dark. Linear; above 1 it blooms. The whole sprite emits.
		glm::vec3 EmissionColor{ 1.0f, 1.0f, 1.0f };
		float EmissionIntensity = 0.0f;
		// 1 = fully shown, 0 = hidden. Cut-out sprites below 1 are screen-door faded (depth stays right), translucent
		// ones blended; the sun shadow is kept. Multiplies Color's alpha.
		float Opacity = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {
		}
	};

	// A built-in MeshComponent shape (MeshFactory). Z-up, standing on its position.
	enum class MeshShape : uint8_t
	{
		Box = 0,
		Plane,
		Ramp,
		Stairs,
		Cylinder
	};

	// A MeshComponent material slot: a shared material asset (.hymat), or the slot's own Inline settings, used when
	// there is no asset or its file is missing.
	struct MeshMaterialSlot
	{
		AssetHandle Material = 0; // a MaterialAsset; 0 = use Inline
		MaterialData Inline;
	};

	// A static mesh, drawn with shared depth when the view's Meshes feature is on.
	struct MeshComponent
	{
		// A Mesh asset (.gltf / .glb); 0 = the built-in Shape.
		AssetHandle Model = 0;
		MeshShape Shape = MeshShape::Box;
		// Width (X), depth (Y), height (Z) in metres, before scale. Built-in shapes stand on the entity's position.
		glm::vec3 Size{ 1.0f };
		uint32_t Steps = 4;     // Stairs
		uint32_t Segments = 16; // Cylinder
		// One per material slot. A model's slots missing here use the model's own imported materials.
		std::vector<MeshMaterialSlot> Materials{ MeshMaterialSlot{} };
		bool CastShadows = true; // into the sun's shadow map (RenderFeatureSettings::Shadows = Sun)
		// 1 = solid, 0 = gone; in between it is dithered so depth and sorting stay right. Its shadow is kept.
		float Opacity = 1.0f;
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent& other) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {
		}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
	};

	struct IDComponent
	{
		UUID Id = {};

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& id)
			: Id(id) {
		}
	};

	// The entity's parent in the hierarchy, by UUID; roots have no component. Change it only through
	// Scene::SetParent / MoveEntity, which keep the entity list depth-first (docs/SceneAndECS.md#hierarchy).
	struct RelationshipComponent
	{
		UUID Parent = 0;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		explicit RelationshipComponent(const UUID& parent)
			: Parent(parent) {
		}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {
		}
		TransformComponent(const glm::vec3& translation, const glm::vec3 scale)
			: Translation(translation), Scale(scale) {
		}
		TransformComponent(const glm::vec3& translation, const glm::vec3 scale, const glm::vec3 rotation)
			: Translation(translation), Scale(scale), Rotation(rotation) {
		}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	// Derived data: the world transform (local TransformComponent combined with every ancestor's), written only by
	// Scene::ResolveTransforms and never saved. Scene::GetWorldTransform is always current; this is as of the last resolve.
	struct WorldTransformComponent
	{
		glm::mat4 Matrix{ 1.0f };
		// Decomposed once so readers don't each decompose it (a mirror is a negative Scale.x).
		glm::vec3 Translation{ 0.0f, 0.0f, 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

		WorldTransformComponent() = default;
		WorldTransformComponent(const WorldTransformComponent&) = default;
	};

	// A camera placed by the entity's TransformComponent (scale ignored); the aspect ratio is the viewport's.
	// SceneRenderer::RenderRuntime uses the first camera with Primary set.
	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	// Playback state of a SpriteAnimatorComponent: not saved, reset when the game starts, written only by
	// UpdateSpriteAnimations and SpriteAnimator. Read it through SpriteAnimator.
	struct SpriteAnimatorState
	{
		int32_t ClipIndex = -1; // -1 = nothing started yet
		uint32_t FrameIndex = 0;
		float FrameTime = 0.0f; // seconds into the current frame
		int8_t Step = 1;        // PingPong direction: 1 forwards, -1 backwards
		bool Playing = false;
		bool Finished = false;  // a Once clip reached its last frame
	};

	// Plays clips of a SpriteAnimationSet on the entity's SpriteRendererComponent (Texture, SpriteIndex, FlipX).
	// Control it with SpriteAnimator; it advances in play mode only.
	struct SpriteAnimatorComponent
	{
		// A SpriteAnimationSet asset (.hyanim, 0 = none), saved as AnimationsHandle.
		AssetHandle Animations = 0;
		std::string StartClip; // played when the game starts; empty = the set's default clip
		bool PlayOnStart = true;
		float Speed = 1.0f;    // 2 = twice as fast

		SpriteAnimatorState State;

		SpriteAnimatorComponent() = default;
		SpriteAnimatorComponent(const SpriteAnimatorComponent&) = default;
	};

	enum class Light2DType : uint8_t
	{
		Point = 0, // lights the area around the entity, out to Radius
		Global,    // lights the whole view equally: the ambient light
		Spot       // forward lighting only: a cone along the entity's -Z, out to Radius
	};

	// A 2D-renderer light. Global lights are the ambient colour (unlit parts are black without one); point lights
	// add to it. Drawn through the sprites' camera, so give it the Z of the sprites it lights.
	struct Light2DComponent
	{
		Light2DType Type = Light2DType::Point;
		glm::vec3 Color{ 1.0f, 1.0f, 1.0f }; // linear RGB
		float Intensity = 1.0f;              // multiplies Color; values above 1 over-brighten (up to 2x the sprite's colour)
		float Radius = 5.0f;                 // Point only: how far the light reaches, in world units
		// Point only: how high above its ground position the light sits, measured up an Upright sprite (a lantern's
		// glass), so it follows the camera's tilt like a point on that sprite. 0 = on the ground.
		float Height = 0.0f;
		// Spot only: cone half angles in degrees (InnerAngle <= OuterAngle). It points along the transform's -Z.
		float InnerAngle = 20.0f;
		float OuterAngle = 30.0f;

		Light2DComponent() = default;
		Light2DComponent(const Light2DComponent&) = default;
	};

	// The sun or moon for forward lighting: parallel light along the transform's -Z. The one marked IsMainLight is
	// used, else the first (Scene::GetMainLight).
	struct DirectionalLightComponent
	{
		glm::vec3 Color{ 1.0f, 0.95f, 0.85f }; // linear RGB
		float Intensity = 1.0f;
		bool IsMainLight = false;
		// Only the main light casts shadows, when the render features have Shadows = Sun.
		bool CastShadows = true;
		float ShadowStrength = 1.0f;  // 0 = no shadow, 1 = no direct sun at all in the shadow (ambient stays)
		float ShadowDistance = 24.0f; // how far from what the camera looks at shadows reach, in metres
		float ShadowBias = 1.0f;      // scales how far receivers are pushed out of their own shadow (acne vs gaps)

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// A soft ellipse that darkens a lit 2D view's light buffer under the entity, grounding the sprite. Size is in
	// world units and ignores scale; it does not move with the lights.
	struct Shadow2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };  // world units from the entity's position (0 when the sprite's pivot is at its feet)
		glm::vec2 Size{ 1.5f, 0.5f };    // ellipse width and height, world units (not scaled)
		float Strength = 0.5f;           // 0 = no shadow, 1 = black at the centre

		Shadow2DComponent() = default;
		Shadow2DComponent(const Shadow2DComponent&) = default;
	};

	enum class Rigidbody2DType : uint8_t
	{
		Static = 0, // never moves; other bodies collide with it
		Dynamic,    // moved by forces, gravity and collisions
		Kinematic   // moved only by the game (velocity or teleport); pushes dynamic bodies but is not pushed
	};

	struct PhysicsMaterial2D
	{
		float Density = 1.0f;     // mass per unit area (a 1 x 1 box of density 1 weighs 1)
		float Friction = 0.3f;    // 0 = ice; values around 0.2 to 1 are usual
		float Restitution = 0.0f; // bounciness: 0 = none, 1 = keeps all its speed
	};

	// A 2D physics body, moved by the simulation (Dynamic) or the game (Kinematic). Colliders without it are static.
	// Data only: nothing simulates it yet.
	struct Rigidbody2DComponent
	{
		Rigidbody2DType Type = Rigidbody2DType::Dynamic;
		bool FixedRotation = false;   // collisions never turn it
		bool IsBullet = false;        // continuous collision, for fast small bodies (projectiles)
		float GravityScale = 1.0f;
		float LinearDamping = 0.0f;   // slows its movement over time
		float AngularDamping = 0.0f;  // slows its turning over time

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	// A 2D box collider in local space: Size is the full width and height at scale 1 (XY scale multiplies it).
	// A sensor detects overlaps but blocks nothing.
	struct BoxCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };
		glm::vec2 Size{ 1.0f, 1.0f };
		PhysicsMaterial2D Material;
		bool IsSensor = false;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	// A 2D circle collider; the larger of the XY scales multiplies Radius and Offset.
	struct CircleCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };
		float Radius = 0.5f;
		PhysicsMaterial2D Material;
		bool IsSensor = false;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	// Which things meet a 3D collider and which queries find it; the entity's first collider decides. Trigger blocks
	// nothing and sends OnTriggerEnter / OnTriggerExit; colliders with IsTrigger are always on it.
	enum class PhysicsLayer : uint8_t
	{
		World = 0,
		Player,
		Npc,
		Trigger
	};

	// A 3D box collider (Jolt). Offset is its centre and Size its full extent, in local space before scale; read when
	// the body is made. Static, or kinematic when the entity has native scripts.
	struct BoxColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.5f };
		glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false; // detects characters walking in and out; blocks nothing
	};

	// A 3D sphere collider; the largest scale multiplies Radius.
	struct SphereColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.5f };
		float Radius = 0.5f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// A 3D capsule along Z, Offset at its centre; Height spans both end caps. XY scale multiplies the radius, Z the height.
	struct CapsuleColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.9f };
		float Radius = 0.3f;
		float Height = 1.8f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// A 3D cylinder along Z (trunks, posts); Offset is its centre.
	struct CylinderColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 1.0f };
		float Radius = 0.3f;
		float Height = 2.0f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// Collides as the MeshComponent: built-in shapes as themselves (Stairs as a ramp, Plane as a thin slab), models
	// through their "_COL" (convex hull) and "_WALK" (triangles) nodes, else their bounding box.
	struct MeshColliderComponent
	{
		PhysicsLayer Layer = PhysicsLayer::World;
	};

	// A character walking the 3D collision world (Jolt CharacterVirtual): a capsule on the entity's feet. Move it
	// with CharacterBody::Move; runtime state lives in PhysicsScene3D.
	struct CharacterBodyComponent
	{
		float Radius = 0.25f;
		float Height = 1.4f;          // feet to the top of the head
		float MaxSlopeDegrees = 50.0f; // steeper is a wall
		float StepHeight = 0.35f;     // the highest step it walks up or down without falling
		// Stops at drops higher than StepHeight instead of falling, as a JRPG field character does.
		bool StaysOnLedges = false;
		PhysicsLayer Layer = PhysicsLayer::Player;
	};

	// A terrain asset with its corner (0, 0) at the entity's position; rotation and scale are ignored.
	struct TerrainComponent
	{
		AssetHandle Terrain = 0; // a Terrain (.hyterrain); 0 = none
		bool CastShadows = true; // into the sun's shadow map (RenderFeatureSettings::Shadows = Sun)
		PhysicsLayer Layer = PhysicsLayer::World; // the layer of its collision
	};

	// How a particle's colour combines with what is already drawn.
	enum class ParticleBlend : uint8_t
	{
		Alpha = 0, // like sprites: smoke, dust, leaves
		Additive   // adds light: sparks, embers, magic
	};

	// Whether lighting darkens a particle. Unlit + Additive stays bright at night.
	enum class ParticleLighting : uint8_t
	{
		Lit = 0, // multiplied by the view's light buffer, like sprites
		Unlit    // not darkened by scene lighting
	};

	// How a tilted camera shows particles. They are simulated in 2D either way.
	enum class ParticleSpace : uint8_t
	{
		Ground = 0, // on the ground plane: dust, leaves, fireflies, splashes
		View        // in the plane facing the camera: X right, Y up the screen,
		            // so bursts spray and fall as they do top-down (sparks, impacts, magic)
	};

	// Where new particles appear around the entity. A cone is a Point with a narrow Spread.
	enum class EmitterShape : uint8_t
	{
		Point = 0,
		Circle, // inside a circle of radius ShapeSize.x
		Box     // inside a ShapeSize.x x ShapeSize.y box
	};

	// A random value between Min and Max (inclusive), picked once per particle.
	struct FloatRange
	{
		float Min = 1.0f;
		float Max = 1.0f;
	};

	// Emits 2D particles in world space. Settings only: live particles will live in the scene's ParticleScene.
	// Saved and edited, but nothing draws it yet.
	struct ParticleEmitter2DComponent
	{
		// The most the editor and the loader allow.
		static constexpr uint32_t MaxParticlesLimit = 4096;

		// Emission
		bool PlayOnStart = true;
		bool Looping = true;
		float Duration = 1.0f;        // seconds per loop
		float Rate = 10.0f;           // particles per second
		uint32_t BurstCount = 0;      // emitted at once at the start of every loop
		uint32_t MaxParticles = 64;   // alive at once, at most (1 to MaxParticlesLimit)
		uint32_t Seed = 0;            // 0 = a new random seed each playback; otherwise repeatable
		EmitterShape Shape = EmitterShape::Point;
		glm::vec2 ShapeSize{ 0.5f, 0.5f }; // Circle: x = radius; Box: width, height (world units)

		// Each particle at birth
		FloatRange Lifetime{ 0.5f, 1.0f };        // seconds
		FloatRange Speed{ 1.0f, 2.0f };           // world units per second
		float Direction = 90.0f;                  // degrees, 0 = right, 90 = up
		float Spread = 30.0f;                     // degrees either side of Direction (180 = every way)
		FloatRange StartSize{ 0.1f, 0.2f };       // world units
		FloatRange StartRotation{ 0.0f, 0.0f };   // degrees
		FloatRange AngularVelocity{ 0.0f, 0.0f }; // degrees per second

		// Over a particle's life
		float EndSizeScale = 1.0f;                // size at death = start size * this
		glm::vec4 StartColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 EndColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		glm::vec2 Gravity{ 0.0f, 0.0f };          // world units / s^2
		float Drag = 0.0f;                        // slows particles down; 0 = none
		float Turbulence = 0.0f;                  // random push, world units / s^2 (wandering fireflies)

		// Look
		AssetHandle Texture = 0;            // 0 = a soft round dot
		uint32_t SpriteIndex = 0;           // for a sprite sheet texture
		ParticleBlend Blend = ParticleBlend::Alpha;
		ParticleLighting Lighting = ParticleLighting::Lit;
		ParticleSpace Space = ParticleSpace::Ground; // with a tilted camera: on the ground or facing the camera
		int32_t SortOrder = 0;            // sorted with sprites (higher = on top)
		float EmissionIntensity = 0.0f;           // a particle's colour times this is added after the lighting (sparks, embers)

		// A light per particle (optional; radius 0 = none)
		float LightRadius = 0.0f;
		float LightIntensity = 1.0f;
		uint32_t MaxLights = 0;                   // the youngest particles that get a light

		// One-shot effects: destroy the entity once it stopped emitting and no particles are left
		bool DestroyWhenFinished = false;

		ParticleEmitter2DComponent() = default;
		ParticleEmitter2DComponent(const ParticleEmitter2DComponent&) = default;
	};

	// ---- Runtime UI (#164; docs/SceneAndECS.md#runtime-ui) ------------------------------------------
	// A canvas is a UICanvasComponent entity; its elements are descendants with a UIRectComponent (canvas pixels, Y up).

	// How a canvas's pixels map to the view's.
	enum class UIScaleMode : uint8_t
	{
		IntegerScale = 0,  // the largest whole multiple of ReferenceResolution that fits, at least 1; leftover space widens the canvas
		ScaleWithScreen,   // a fractional scale that fits ReferenceResolution (smooth UI)
		ConstantPixelSize  // 1 canvas pixel = 1 view pixel
	};

	// Where a canvas is.
	enum class UICanvasSpace : uint8_t
	{
		Screen = 0, // covers the view
		World       // a WorldSize box pinned to the entity's projected position,
		            // the same size on screen at any distance: labels, enemy info, prompts
	};

	struct UICanvasComponent
	{
		// Canvas pixels the layout is designed for (sets the scale). The game uses 1920 x 1080 scaled smoothly;
		// 480 x 270 IntegerScale suits pixel UI.
		glm::vec2 ReferenceResolution{ 1920.0f, 1080.0f };
		UIScaleMode ScaleMode = UIScaleMode::ScaleWithScreen;
		UICanvasSpace Space = UICanvasSpace::Screen;
		glm::vec2 WorldSize{ 320.0f, 80.0f };  // World: the canvas's size in canvas pixels
		glm::vec2 WorldPivot{ 0.5f, 0.0f };    // World: the point of the box (0..1) that sits on the entity's position
		int32_t SortOrder = 0;            // canvases with a higher order draw on top
		bool Visible = true;                // false hides the canvas and everything on it
		bool BlocksGameplayInput = false; // while visible, gameplay scripts don't get keyboard input
		bool DrawDebug = false;           // outlines of the canvas and its rects in the Game view

		UICanvasComponent() = default;
		UICanvasComponent(const UICanvasComponent&) = default;
	};

	// Where an element sits in its parent's rect (Unity's RectTransform, Y up). Authored, never written by a system;
	// the result is UIResolvedRectComponent.
	struct UIRectComponent
	{
		glm::vec2 AnchorMin{ 0.5f, 0.5f };  // corners in the parent rect, 0..1 (0,0 = bottom-left)
		glm::vec2 AnchorMax{ 0.5f, 0.5f };
		glm::vec2 Pivot{ 0.5f, 0.5f };      // the point AnchoredPosition places, as a fraction of the rect's size
		glm::vec2 AnchoredPosition{ 0.0f, 0.0f }; // the pivot's offset from the anchors, canvas pixels
		glm::vec2 SizeDelta{ 100.0f, 100.0f };   // the size added to the anchors' span (the size itself when the anchors meet)
		bool Visible = true;                // false hides the element and its subtree

		UIRectComponent() = default;
		UIRectComponent(const UIRectComponent&) = default;
	};

	// Derived data: the laid-out rect, written only by Scene::ResolveUILayout, never saved.
	struct UIResolvedRectComponent
	{
		glm::vec2 Min{ 0.0f, 0.0f };        // canvas pixels, Y up, whole pixels
		glm::vec2 Max{ 0.0f, 0.0f };
		bool Visible = false;               // its own Visible, every ancestor's and the canvas's
		UUID Canvas = 0;                    // the canvas it is drawn on; 0 = none (no canvas above it, or a nested one)
		// The visible part, cut by scroll views above it (canvas pixels); drawing and the pointer are cut to it.
		glm::vec2 ClipMin{ -1.0e9f, -1.0e9f };
		glm::vec2 ClipMax{ 1.0e9f, 1.0e9f };

		UIResolvedRectComponent() = default;
		UIResolvedRectComponent(const UIResolvedRectComponent&) = default;
	};

	enum class UILayoutDirection : uint8_t { Vertical = 0, Horizontal };
	enum class UILayoutAlignment : uint8_t { Start = 0, Center, End }; // Start = top (vertical) / left (horizontal)

	// Stacks the visible UI children one after another inside the rect less Padding. Only the resolved rects change,
	// so removing the group restores the authored layout. FitContent sizes the group to its children.
	struct UILayoutGroupComponent
	{
		UILayoutDirection Direction = UILayoutDirection::Vertical;
		float Spacing = 2.0f;                        // canvas pixels between children
		glm::vec4 Padding{ 0.0f, 0.0f, 0.0f, 0.0f }; // left, bottom, right, top, canvas pixels
		UILayoutAlignment ChildAlignment = UILayoutAlignment::Start; // along the stack and, unstretched, across it
		bool StretchCrossAxis = true;                // children take the content's width (vertical) / height (horizontal)
		bool FitContent = false;

		UILayoutGroupComponent() = default;
		UILayoutGroupComponent(const UILayoutGroupComponent&) = default;
	};

	// An image filling the element's rect: a texture (a sprite of a sheet), or a flat colour without one.
	struct UIImageComponent
	{
		AssetHandle Texture = 0;            // 0 = a flat colour; load it before assigning (the renderer never loads)
		uint32_t SpriteIndex = 0;           // for a sprite sheet texture
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Nine-slice borders in texture pixels: left, bottom, right, top (#81); all 0 = stretch it all.
		glm::vec4 NineSlice{ 0.0f, 0.0f, 0.0f, 0.0f };
		bool FlipX = false;
		bool FlipY = false;

		UIImageComponent() = default;
		UIImageComponent(const UIImageComponent&) = default;
	};

	// Text in the element's rect in a bitmap font (laid out by TextLayout, drawn by SceneRenderer::SubmitUI).
	struct UITextComponent
	{
		std::string Text;                   // UTF-8; \n breaks a line
		AssetHandle Font = 0;               // a Font asset (.fnt); load it before assigning (the renderer never loads)
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t Scale = 1;                 // whole multiples of the font's pixel size
		TextAlignH AlignH = TextAlignH::Left;
		TextAlignV AlignV = TextAlignV::Top;
		bool Wrap = true;                   // at the rect's width, on spaces
		float LineSpacing = 1.0f;           // times the font's line height
		int32_t VisibleCharacters = -1;     // -1 = all; a typewriter counts this up (the layout never changes)
		glm::vec4 ShadowColor{ 0.0f, 0.0f, 0.0f, 0.0f }; // a drop shadow under the glyphs; alpha 0 = none
		glm::vec2 ShadowOffset{ 2.0f, -2.0f };           // canvas pixels, Y up

		UITextComponent() = default;
		UITextComponent(const UITextComponent&) = default;
	};

	// How keyboard focus finds the next button.
	enum class UINavigation : uint8_t
	{
		Auto = 0, // the nearest interactable button in that direction, on the same canvas
		Explicit, // Up / Down / Left / Right
		None
	};

	// Makes the element focusable and pressable; the state tints apply to its UIImageComponent and UITextComponent.
	struct UIButtonComponent
	{
		bool Interactable = true;
		std::string Event;                  // the name sent with Submit, e.g. "UseItem", "Attack"
		glm::vec4 NormalTint{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 FocusedTint{ 1.0f, 0.95f, 0.75f, 1.0f };
		glm::vec4 PressedTint{ 0.8f, 0.8f, 0.8f, 1.0f };
		glm::vec4 DisabledTint{ 1.0f, 1.0f, 1.0f, 0.4f };
		UINavigation Navigation = UINavigation::Auto;
		// Explicit navigation's neighbours by UUID (0 = none); Scene::RemapEntityReferences keeps them inside copies.
		UUID Up = 0;
		UUID Down = 0;
		UUID Left = 0;
		UUID Right = 0;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};

	// A vertical scrolling area: its children move up by ScrollPosition and are cut to its rect. With FollowFocus the
	// layout keeps the focused button in view. ScrollPosition is saved as the starting position.
	struct UIScrollViewComponent
	{
		float ScrollPosition = 0.0f;        // canvas pixels the content is moved up by
		bool FollowFocus = true;
		// The scrollbar thumb element by UUID (0 = none); the layout sizes and moves it, hidden while everything fits.
		UUID Scrollbar = 0;

		UIScrollViewComponent() = default;
		UIScrollViewComponent(const UIScrollViewComponent&) = default;
	};

	// When an element shows, by the state of its button (itself or the nearest one above): a focused-row cursor or
	// a resting look the focused row drops.
	enum class UIFocusVisibility : uint8_t
	{
		Focused = 0, // while the button is focused or pressed
		Unfocused    // while it isn't
	};

	struct UIFocusVisibilityComponent
	{
		UIFocusVisibility Show = UIFocusVisibility::Focused;

		UIFocusVisibilityComponent() = default;
		UIFocusVisibilityComponent(const UIFocusVisibilityComponent&) = default;
	};
}
