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

	// How a sprite's quad is turned towards the camera. The ground is the XY plane and +Z is up (Diorama RPG
	// H1), so a top-down camera (looking down -Z) sees every facing the same way: as a flat quad in XY.
	enum class SpriteFacing : uint8_t
	{
		Flat = 0,  // lies on the ground (XY), as it is authored: ground, roads, decals, water, bridges
		Upright,   // stands on the bottom edge of its quad and turns to face the camera (yaw and pitch, never
		           // roll): trees, buildings, props, characters. With a top-down camera it lies flat, so
		           // tilting the camera stands it up gradually
		Billboard  // turns about its centre to face the camera fully: particles, glows, icons, world-space UI
	};

	// How a sprite is drawn in a view with shared depth (RenderFeatureSettings::Depth). Without it every sprite
	// is translucent and drawn in sort order, as in the 2D preset.
	enum class SpriteAlphaMode : uint8_t
	{
		Default = 0,  // the view's RenderFeatureSettings::DefaultSpriteAlpha (translucent in the 2D preset)
		Cutout,       // pixels under half alpha are dropped, the rest are opaque: depth test and write, no blending
		Translucent   // alpha blended: depth test, no depth write, sorted back to front (glows, water, effects)
	};

	// A quad drawn with the entity's TransformComponent: Texture (white when 0 or not loaded) multiplied by Color
	// (alpha blended). A Single texture is drawn whole, repeated TilingFactor times; a Multiple texture
	// (a sprite sheet, see TextureImportConfig) draws its sprite SpriteIndex, placed by the sprite's pivot.
	// Facing decides how the quad turns towards a tilted camera (SceneRenderer).
	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// The image to draw: the AssetHandle of a Texture2D in the project's asset folder (0 = none).
		// Saved as TextureHandle. SceneRenderer draws it when the AssetManager has it loaded
		// (GetLoadedAsset), so whatever assigns a handle makes sure it is loaded: loading a scene and the
		// editor's pickers do; game code calls AssetManager::GetAsset<Texture2D>(handle) once.
		AssetHandle Texture = 0;
		float TilingFactor = 1.0f; // Single textures only
		// Draw order: sprites are drawn from low to high SortOrder, so higher values end up on top.
		// Equal values: Flat sprites first (lower Translation.z first), then Upright and Billboard ones from
		// far to near along the camera's ground direction (feet of an Upright, centre of a Billboard), then
		// the order the scene stores them in.
		int32_t SortOrder = 0;
		// Which of the texture's sprites to draw (Texture2D::GetSprite). 0 is the whole image of a
		// Single texture and the first sprite of a Multiple one; out of range draws sprite 0.
		// A playing SpriteAnimatorComponent sets it every frame.
		uint32_t SpriteIndex = 0;
		bool FlipX = false;
		bool FlipY = false;
		SpriteFacing Facing = SpriteFacing::Flat;
		SpriteAlphaMode AlphaMode = SpriteAlphaMode::Default;
		// HUD: drawn in the view's overlay pass, after everything else and the lighting: unlit, never depth
		// tested, in SortOrder among the overlay sprites. For world-space prompts and menus until runtime UI.
		bool Overlay = false;
		// Sun shadows: a cut-out Upright or Billboard sprite casts one (RenderFeatureSettings::SpriteShadows);
		// off for grass, flowers and anything that shouldn't darken the ground.
		bool CastShadows = true;
		// Emission (post-processing plan P3): the sprite's own pixels (texture times Color) times EmissionColor
		// times EmissionIntensity are added after the lighting, so a flame or a lit window glows at night and in
		// shadow. Linear; above 1 it blooms (with the Bloom render feature). 0 = none, as before. The whole sprite
		// emits: a torch is a body sprite and a flame sprite.
		glm::vec3 EmissionColor{ 1.0f, 1.0f, 1.0f };
		float EmissionIntensity = 0.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {
		}
	};

	// A built-in shape of a MeshComponent (MeshFactory). Z-up, standing on its position.
	enum class MeshShape : uint8_t
	{
		Box = 0,
		Plane,
		Ramp,
		Stairs,
		Cylinder
	};

	// One material slot of a MeshComponent: a material asset (.hymat, plan M4b), shared with every slot that
	// names it, or the slot's own settings. Inline is drawn when there is no asset, or its file is missing.
	struct MeshMaterialSlot
	{
		AssetHandle Material = 0; // a MaterialAsset; 0 = use Inline
		MaterialData Inline;
	};

	// A static mesh drawn with shared depth when the view's render features have Meshes on (plan M4).
	// Plain data: SceneRenderer turns it into a cached Mesh and materials every frame.
	struct MeshComponent
	{
		// A model (a Mesh asset: .gltf / .glb, M5); 0 = the built-in Shape below.
		AssetHandle Model = 0;
		MeshShape Shape = MeshShape::Box;
		// Width (X), depth (Y), height (Z) in metres, before the transform's scale. Built-in shapes stand on
		// the entity's position (their base is centred there at z = 0).
		glm::vec3 Size{ 1.0f };
		uint32_t Steps = 4;     // Stairs
		uint32_t Segments = 16; // Cylinder
		// One per material slot (built-in shapes have one; a model has one per glTF material). A model's slots
		// that aren't here use the model's own materials (the assets its import made): empty = all of them.
		std::vector<MeshMaterialSlot> Materials{ MeshMaterialSlot{} };
		bool CastShadows = true; // into the sun's shadow map (RenderFeatureSettings::Shadows = Sun)
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

	// The entity's parent in the scene's hierarchy (Roadmap S3), by UUID. An entity without this component is
	// a root; the component is removed rather than set to 0. Saved with scenes and copied on Play.
	// Change it only through Scene::SetParent / Scene::MoveEntity: they keep the scene's entity list in
	// depth-first order, which the hierarchy relies on (docs/SceneAndECS.md#hierarchy).
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

	// The entity's transform in the world: its TransformComponent (local, relative to its parent) combined with
	// every ancestor's. Derived data: added and removed together with the TransformComponent (Scene connects
	// to the registry's construct / destroy signals), written only by Scene::ResolveTransforms, never saved,
	// copied on Play or shown in the inspector. Read it instead of TransformComponent wherever the position in
	// the world matters (rendering, lights, particles, the editor's outlines). It is as of the last ResolveTransforms
	// (stage 4, the start of the game, the start of rendering); Scene::GetWorldTransform is always current.
	struct WorldTransformComponent
	{
		glm::mat4 Matrix{ 1.0f };
		// Matrix split once here, so readers don't each decompose it (TransformMath::Decompose; a mirror is a
		// negative Scale.x).
		glm::vec3 Translation{ 0.0f, 0.0f, 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

		WorldTransformComponent() = default;
		WorldTransformComponent(const WorldTransformComponent&) = default;
	};

	// A camera the scene can be viewed through. Its position and rotation come from the entity's
	// TransformComponent (scale is ignored), and its aspect ratio from the scene's viewport.
	// SceneRenderer::RenderRuntime draws the scene from the first camera with Primary set.
	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	// Playback of a SpriteAnimatorComponent: runtime state kept in the component (component state rule 2
	// in docs/SceneAndECS.md). Not saved; reset for every animator when the game starts
	// (ResetSpriteAnimations); written only by UpdateSpriteAnimations and SpriteAnimator. Everything else
	// reads it through SpriteAnimator (GetCurrentClip, GetFrameIndex, IsPlaying, IsFinished).
	struct SpriteAnimatorState
	{
		int32_t ClipIndex = -1; // -1 = nothing started yet
		uint32_t FrameIndex = 0;
		float FrameTime = 0.0f; // seconds into the current frame
		int8_t Step = 1;        // PingPong direction: 1 forwards, -1 backwards
		bool Playing = false;
		bool Finished = false;  // a Once clip reached its last frame
	};

	// Plays clips from a SpriteAnimationSet on the entity's SpriteRendererComponent (it sets the
	// sprite's Texture, SpriteIndex and FlipX). Control it with SpriteAnimator (Play, Stop, Pause...);
	// UpdateSpriteAnimations advances it, in play mode only.
	struct SpriteAnimatorComponent
	{
		// Saved. The AssetHandle of a SpriteAnimationSet (.hyanim) in the asset folder (0 = none), as
		// SpriteRendererComponent::Texture. Saved as AnimationsHandle.
		AssetHandle Animations = 0;
		std::string StartClip; // played when the game starts; empty = the set's default clip
		bool PlayOnStart = true;
		float Speed = 1.0f;    // 2 = twice as fast

		// Runtime, not saved: see SpriteAnimatorState.
		SpriteAnimatorState State;

		SpriteAnimatorComponent() = default;
		SpriteAnimatorComponent(const SpriteAnimatorComponent&) = default;
	};

	enum class Light2DType : uint8_t
	{
		Point = 0, // lights the area around the entity, out to Radius
		Global,    // lights the whole view equally: the ambient light
		Spot       // forward lighting only (M6): a cone along the entity's -Z, out to Radius
	};

	// A light for the 2D renderer. A point light is placed by the entity's TransformComponent; a global
	// light ignores the position. SceneRenderer submits every light to Renderer2D, which draws a view
	// that has at least one light lit (RendererSettings::Lighting): the global lights are the ambient
	// colour of the scene, point lights add to it. Without a global light, the unlit parts are black.
	// The light is drawn through the same camera as the sprites, so give it the Z of the sprites it lights.
	struct Light2DComponent
	{
		Light2DType Type = Light2DType::Point;
		glm::vec3 Color{ 1.0f, 1.0f, 1.0f }; // linear RGB
		float Intensity = 1.0f;              // multiplies Color; values above 1 over-brighten (up to 2x the sprite's colour)
		float Radius = 5.0f;                 // Point only: how far the light reaches, in world units
		// Point only: how high above its ground position the light sits, measured up an Upright sprite (a
		// lantern's glass, a window). The light moves with the camera's tilt the way a point on an Upright
		// sprite does: Height units up the screen in a top-down view, that far up the standing sprite in a
		// tilted one. 0 = on the ground.
		float Height = 0.0f;
		// Spot only: the cone's half angles in degrees; full light inside InnerAngle, none outside OuterAngle
		// (InnerAngle <= OuterAngle). Its direction is the transform's -Z (a lamp over a door points down).
		float InnerAngle = 20.0f;
		float OuterAngle = 30.0f;

		Light2DComponent() = default;
		Light2DComponent(const Light2DComponent&) = default;
	};

	// The sun or moon of a scene with forward lighting (plan M6): parallel light shining along the
	// transform's -Z (rotate the entity to aim it). One per scene is used: the one marked IsMainLight, else
	// the first (Scene::GetMainLight). Ignored by the 2D light buffer.
	struct DirectionalLightComponent
	{
		glm::vec3 Color{ 1.0f, 0.95f, 0.85f }; // linear RGB
		float Intensity = 1.0f;
		bool IsMainLight = false;
		// Sun shadows (the main light only, when the render features have Shadows = Sun).
		bool CastShadows = true;
		float ShadowStrength = 1.0f;  // 0 = no shadow, 1 = no direct sun at all in the shadow (ambient stays)
		float ShadowDistance = 24.0f; // metres from what the camera looks at that get shadows (the map's half width)
		float ShadowBias = 1.0f;      // scales how far receivers are pushed out of their own shadow (acne vs. gaps)

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// A soft blob shadow under the entity, for the 2D renderer: an ellipse that darkens the light buffer of a
	// lit view (RendererSettings::Shadows). Placed by the TransformComponent plus Offset; Size is in world
	// units and ignores the transform's scale. It is not cast from lights, so it does not move with them: it grounds the
	// sprite. It darkens whatever light reaches that spot, ambient included.
	struct Shadow2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };  // from the entity's position, in world units (0 for a sprite whose pivot is at its feet)
		glm::vec2 Size{ 1.5f, 0.5f };    // width and height of the ellipse, in world units (not scaled by the transform)
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

	// How a collider's surface behaves. Shared by the 2D collider components.
	struct PhysicsMaterial2D
	{
		float Density = 1.0f;     // mass per unit area (a 1 x 1 box of density 1 weighs 1)
		float Friction = 0.3f;    // 0 = ice; values around 0.2 to 1 are usual
		float Restitution = 0.0f; // bounciness: 0 = none, 1 = keeps all its speed
	};

	// Makes the entity a body in the 2D physics world while the game plays, moved by the simulation
	// (Dynamic) or by the game (Kinematic). Position and angle come from the TransformComponent
	// (Translation.xy and Rotation.z). An entity with a collider but no Rigidbody2D is a static body.
	// Data only for now: nothing simulates it yet.
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

	// A rectangle that collides, in the entity's local space. Size is the full width and height in world
	// units at scale 1: the entity's XY scale multiplies it (and Offset) when the body is created.
	// IsSensor: it detects overlaps (trigger events) but nothing bounces off it.
	struct BoxCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };
		glm::vec2 Size{ 1.0f, 1.0f };
		PhysicsMaterial2D Material;
		bool IsSensor = false;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	// A circle that collides. Radius is in world units at scale 1; the larger of the entity's X and Y
	// scales multiplies it (and Offset) when the body is created.
	struct CircleCollider2DComponent
	{
		glm::vec2 Offset{ 0.0f, 0.0f };
		float Radius = 0.5f;
		PhysicsMaterial2D Material;
		bool IsSensor = false;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	// What a 3D collider is, for which things meet it (PhysicsScene3D) and which a query looks for (Physics3D).
	// One layer per entity: the first of its colliders decides.
	//  - World: ground, walls and props. Characters stand on it and are blocked by it.
	//  - Player, Npc: what characters and the things that walk with them are.
	//  - Trigger: an area that blocks nothing; queries find it, and characters walking in and out of it send
	//    OnTriggerEnter / OnTriggerExit. Colliders with IsTrigger are always on this layer.
	enum class PhysicsLayer : uint8_t
	{
		World = 0,
		Player,
		Npc,
		Trigger
	};

	// A box that collides in the 3D physics world (Jolt) while the game plays. Offset is the box's centre and
	// Size its full width (X), depth (Y) and height (Z), both in the entity's local space before its scale: the
	// transform's scale multiplies them and its rotation turns the box when the body is made (changing them later
	// has no effect on the body). An entity with 3D colliders is a static body, or a kinematic one that follows
	// its transform when it has native scripts (it moves). The 3D colliders are read by Physics/PhysicsScene3D;
	// their shapes come from Scene/ColliderShapes.
	struct BoxColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.5f };
		glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false; // detects characters walking in and out; blocks nothing
	};

	// A sphere that collides (3D). Radius in world units at scale 1: the largest of the entity's scales multiplies it.
	struct SphereColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.5f };
		float Radius = 0.5f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// A capsule standing along the entity's Z (3D). Height is from the bottom of the lower half sphere to the top
	// of the upper one; Offset is its centre. The larger of the X and Y scales multiplies the radius, Z the height.
	struct CapsuleColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 0.9f };
		float Radius = 0.3f;
		float Height = 1.8f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// A cylinder standing along the entity's Z (3D): a tree trunk, a post. Offset is its centre.
	struct CylinderColliderComponent
	{
		glm::vec3 Offset{ 0.0f, 0.0f, 1.0f };
		float Radius = 0.3f;
		float Height = 2.0f;
		PhysicsLayer Layer = PhysicsLayer::World;
		bool IsTrigger = false;
	};

	// The entity's MeshComponent collides as what it is (3D). A built-in shape collides as its own shape, except
	// Stairs, which collide as a ramp through the middle of their treads (smooth to walk, the camera doesn't bob)
	// and Plane, a thin slab under its surface. A model collides through its collision nodes (World collision plan
	// WC3): every node named "..._COL" as a solid convex hull and every "..._WALK" as walkable triangles, both
	// authored in Blender next to the drawn mesh and never drawn. A model without such nodes collides as its
	// bounding box. It needs the model loaded. Never the render triangles.
	struct MeshColliderComponent
	{
		PhysicsLayer Layer = PhysicsLayer::World;
	};

	// A character that walks over the 3D collision world (Jolt's CharacterVirtual): a capsule standing on the
	// entity's position (its feet), that slides along walls, walks up slopes up to MaxSlopeDegrees and steps up
	// and down StepHeight, and sticks to the ground. Game code says where it wants to go with
	// CharacterBody::Move (Physics/Physics3D.h); the physics stage moves it and writes the result to the
	// transform, blended between fixed steps so it moves smoothly at any frame rate. The capsule never depends on
	// the sprite or its animation. Runtime state lives in PhysicsScene3D, not here.
	struct CharacterBodyComponent
	{
		float Radius = 0.25f;
		float Height = 1.4f;          // feet to the top of the head
		float MaxSlopeDegrees = 50.0f; // steeper is a wall
		float StepHeight = 0.35f;     // the highest step it walks up or down without falling
		// It never walks off a drop higher than StepHeight: at a terrace's edge it stops (or slides along the edge)
		// instead of falling, as a JRPG field character does. Off: it walks off and falls.
		bool StaysOnLedges = false;
		PhysicsLayer Layer = PhysicsLayer::Player;
	};

	// Every engine component that is plain, copyable data, apart from TagComponent and IDComponent
	// (every entity has those). Scene::Copy copies these; add new copyable components here.
	// How a particle's colour is combined with what is already drawn.
	enum class ParticleBlend : uint8_t
	{
		Alpha = 0, // like sprites: smoke, dust, leaves
		Additive   // adds light: sparks, embers, magic
	};

	// Whether the view's lighting darkens a particle. Unlit + Additive is "emissive" (stays bright at night).
	enum class ParticleLighting : uint8_t
	{
		Lit = 0, // multiplied by the view's light buffer, like sprites
		Unlit    // not darkened by scene lighting (Roadmap V4; drawn lit until then)
	};

	// How a tilted camera shows the particles. They are simulated in 2D either way (X, Y and Gravity, as authored).
	enum class ParticleSpace : uint8_t
	{
		Ground = 0, // on the ground plane, where they move: dust, leaves, fireflies, puddle splashes (as in a top-down view)
		View        // in the plane facing the camera through the emitter: X right and Y up the screen, so a burst
		            // sprays and falls like it does top-down (hit sparks, impacts, magic; H10 gives Ground a height)
	};

	// Where new particles appear, around the entity's position. A cone is a Point with a narrow Spread.
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

	// Emits 2D particles at the entity's position (world space: a particle stays where it was emitted).
	// Settings only (component state rule 3 in docs/SceneAndECS.md): the live particles and the playback
	// state will be in the scene's ParticleScene, simulated by UpdateParticles and controlled with
	// Particles2D (Roadmap V2, V5). Until then this component is saved, copied on Play and edited in the
	// inspector, but nothing is drawn.
	struct ParticleEmitter2DComponent
	{
		// Limits the editor and the loader clamp to.
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

		// Over a particle's life (start to end; curves are Roadmap V3)
		float EndSizeScale = 1.0f;                // size at death = start size * this
		glm::vec4 StartColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 EndColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		glm::vec2 Gravity{ 0.0f, 0.0f };          // world units / s^2
		float Drag = 0.0f;                        // slows particles down; 0 = none
		float Turbulence = 0.0f;                  // random push, world units / s^2 (wandering fireflies)

		// Look
		AssetHandle Texture = 0;                  // 0 = a soft round dot
		uint32_t SpriteIndex = 0;                 // for a sprite sheet texture
		ParticleBlend Blend = ParticleBlend::Alpha;
		ParticleLighting Lighting = ParticleLighting::Lit;
		ParticleSpace Space = ParticleSpace::Ground; // with a tilted camera: on the ground or facing the camera
		int32_t SortOrder = 0;                    // sorted with sprites (higher = on top)
		float EmissionIntensity = 0.0f;           // a particle's own colour times this is added after the lighting (sparks, embers; plan P3)

		// Light per emitter (Roadmap V4, optional; 0 radius = none)
		float LightRadius = 0.0f;
		float LightIntensity = 1.0f;
		uint32_t MaxLights = 0;                   // the youngest particles that get a light

		// One-shot effects: destroy the entity once it stopped emitting and no particles are left
		bool DestroyWhenFinished = false;

		ParticleEmitter2DComponent() = default;
		ParticleEmitter2DComponent(const ParticleEmitter2DComponent&) = default;
	};
	// ---- Runtime UI (#164; docs/SceneAndECS.md#runtime-ui) ------------------------------------------
	// A canvas is an entity with a UICanvasComponent, normally a root; its UI elements are the descendants with a
	// UIRectComponent. Elements are placed by their rect (resolved in canvas pixels, Y up, by Scene::ResolveUILayout),
	// never by a TransformComponent, and drawn in hierarchy order on top of the scene (SceneRenderer, the Game view).

	// How a canvas's pixels map to the view's.
	enum class UIScaleMode : uint8_t
	{
		IntegerScale = 0,  // the largest whole multiple of ReferenceResolution that fits, at least 1; leftover space widens the canvas
		ScaleWithScreen,   // a fractional scale that fits ReferenceResolution (smooth UI)
		ConstantPixelSize  // 1 canvas pixel = 1 view pixel
	};

	// Where a canvas is (UI6).
	enum class UICanvasSpace : uint8_t
	{
		Screen = 0, // covers the view
		World       // a WorldSize box pinned to the entity's world position (projected through the primary camera),
		            // the same size on screen however far away: labels over characters, enemy info, prompts
	};

	struct UICanvasComponent
	{
		// Canvas pixels the layout is designed for (sets the scale). The game's UI is a 1920 x 1080 canvas scaled
		// smoothly with the view, with smooth fonts (decision D5, Oct 30, 2026); 480 x 270 IntegerScale suits pixel UI.
		glm::vec2 ReferenceResolution{ 1920.0f, 1080.0f };
		UIScaleMode ScaleMode = UIScaleMode::ScaleWithScreen;
		UICanvasSpace Space = UICanvasSpace::Screen;
		glm::vec2 WorldSize{ 320.0f, 80.0f };  // World: the canvas's size in canvas pixels
		glm::vec2 WorldPivot{ 0.5f, 0.0f };    // World: the point of the box (0..1) that sits on the entity's position
		int32_t SortOrder = 0;            // canvases with a higher order draw on top
		bool Visible = true;              // false hides the canvas and everything on it
		bool BlocksGameplayInput = false; // while visible, gameplay scripts don't get keyboard input (UI3a)
		bool DrawDebug = false;           // outlines of the canvas and its rects in the Game view

		UICanvasComponent() = default;
		UICanvasComponent(const UICanvasComponent&) = default;
	};

	// Where an element sits in its parent's rect (the canvas, for a direct child): Unity's RectTransform, Y up.
	// Authored: saved, copied and edited; never written by a system (the result is UIResolvedRectComponent).
	struct UIRectComponent
	{
		glm::vec2 AnchorMin{ 0.5f, 0.5f };  // corners in the parent rect, 0..1 (0,0 = bottom-left)
		glm::vec2 AnchorMax{ 0.5f, 0.5f };
		glm::vec2 Pivot{ 0.5f, 0.5f };      // the point AnchoredPosition places, as a fraction of the rect's size
		glm::vec2 AnchoredPosition{ 0.0f, 0.0f }; // the pivot's offset from the anchors (where AnchorMin..Max puts it), canvas pixels
		glm::vec2 SizeDelta{ 100.0f, 100.0f };   // the size added to the anchors' span (the size itself when the anchors meet)
		bool Visible = true;                // false hides the element and its subtree

		UIRectComponent() = default;
		UIRectComponent(const UIRectComponent&) = default;
	};

	// An element's rect as laid out. Derived data, like WorldTransformComponent: added and removed together with the
	// UIRectComponent (registry signals), written only by Scene::ResolveUILayout, never saved, copied or edited.
	struct UIResolvedRectComponent
	{
		glm::vec2 Min{ 0.0f, 0.0f };        // canvas pixels, Y up, whole pixels
		glm::vec2 Max{ 0.0f, 0.0f };
		bool Visible = false;               // its own Visible, every ancestor's and the canvas's
		UUID Canvas = 0;                    // the canvas it is drawn on; 0 = none (no canvas above it, or a nested one)
		// What of it can show: the scroll views above it (canvas pixels). Drawing and the pointer are cut to it.
		glm::vec2 ClipMin{ -1.0e9f, -1.0e9f };
		glm::vec2 ClipMax{ 1.0e9f, 1.0e9f };

		UIResolvedRectComponent() = default;
		UIResolvedRectComponent(const UIResolvedRectComponent&) = default;
	};

	enum class UILayoutDirection : uint8_t { Vertical = 0, Horizontal };
	// Where children sit along an axis of a layout group's content rect.
	enum class UILayoutAlignment : uint8_t { Start = 0, Center, End }; // Start = top (vertical) / left (horizontal)

	// Stacks the element's visible UI children one after another (UI4), top to bottom or left to right, inside its rect
	// less Padding. It only changes their resolved rects: their UIRectComponents keep what was authored (the inspector
	// shows their placement as driven), so removing the group puts every child back where it was. A child's size
	// along the stack is its own resolved size; across it, the content's when StretchCrossAxis, else its own, placed by
	// ChildAlignment. Hidden children take no room. With FitContent the group's own size along the stack follows its
	// children (its pivot stays put), so a list grows and shrinks with its rows.
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
		AssetHandle Texture = 0;            // 0 = a flat colour. Load it before assigning (the renderer never loads)
		uint32_t SpriteIndex = 0;           // for a sprite sheet texture
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// Nine-slice borders in texture pixels: left, bottom, right, top (#81). The corners keep their size (1 texture
		// pixel = 1 canvas pixel), the edges stretch along one axis and the centre along both; all 0 = stretch it all.
		glm::vec4 NineSlice{ 0.0f, 0.0f, 0.0f, 0.0f };
		bool FlipX = false;
		bool FlipY = false;

		UIImageComponent() = default;
		UIImageComponent(const UIImageComponent&) = default;
	};

	// Text in the element's rect, in a bitmap font (Font, laid out by TextLayout, drawn by SceneRenderer::SubmitUI).
	// The alignment enums are TextLayout's (TextAlignH / TextAlignV).
	struct UITextComponent
	{
		std::string Text;                   // UTF-8; \n breaks a line
		AssetHandle Font = 0;               // a Font asset (.fnt). Load it before assigning (the renderer never loads)
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

	// How a button finds the next one when focus moves with the keyboard (UI3a).
	enum class UINavigation : uint8_t
	{
		Auto = 0, // the nearest interactable button in that direction, on the same canvas
		Explicit, // Up / Down / Left / Right
		None
	};

	// Makes the element focusable and pressable (UI3a, UI3b; UI1 saves and edits it). The tints are applied to the
	// entity's UIImageComponent and UITextComponent by state.
	struct UIButtonComponent
	{
		bool Interactable = true;
		std::string Event;                  // the name sent with Submit, e.g. "UseItem", "Attack"
		glm::vec4 NormalTint{ 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 FocusedTint{ 1.0f, 0.95f, 0.75f, 1.0f };
		glm::vec4 PressedTint{ 0.8f, 0.8f, 0.8f, 1.0f };
		glm::vec4 DisabledTint{ 1.0f, 1.0f, 1.0f, 0.4f };
		UINavigation Navigation = UINavigation::Auto;
		// Explicit navigation's neighbours, by UUID (0 = none); Scene::RemapEntityReferences keeps them inside copies.
		UUID Up = 0;
		UUID Down = 0;
		UUID Left = 0;
		UUID Right = 0;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};

	// A vertical scrolling area (UI6): its direct children (the content, typically one fitted layout group anchored at
	// the top) move up by ScrollPosition, and everything below it is cut to its rect when drawn and hit-tested.
	// ScrollPosition is kept between 0 and how much taller the content is than the view; with FollowFocus,
	// Scene::ResolveUILayout scrolls so the focused button inside it is always whole in view (the art guide: focus
	// never scrolls off-screen). It is runtime state the layout writes, like Unity's ScrollRect position: saved as
	// the starting position.
	struct UIScrollViewComponent
	{
		float ScrollPosition = 0.0f;        // canvas pixels the content is moved up by
		bool FollowFocus = true;
		// An element outside the view (in a track beside it) that the layout sizes and moves as the scrollbar's thumb
		// in its parent's rect, by UUID (0 = none); hidden while everything fits.
		UUID Scrollbar = 0;

		UIScrollViewComponent() = default;
		UIScrollViewComponent(const UIScrollViewComponent&) = default;
	};

	// When an element shows, by the state of the button it belongs to (itself or the nearest one above it, UI6): a
	// cursor or a highlight that only the focused row has, or a resting look the focused row drops.
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

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using CopyableComponents = ComponentGroup<RelationshipComponent, TransformComponent, SpriteRendererComponent, CameraComponent, SpriteAnimatorComponent, Light2DComponent, Shadow2DComponent,
		Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, ParticleEmitter2DComponent, MeshComponent, DirectionalLightComponent,
		BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, CylinderColliderComponent, MeshColliderComponent, CharacterBodyComponent, UICanvasComponent, UIRectComponent, UIImageComponent, UIButtonComponent, UITextComponent, UILayoutGroupComponent, UIScrollViewComponent, UIFocusVisibilityComponent>;
}
