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
		bool CastShadows = true; // kept for shadows (H6); nothing reads it yet
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
		Global     // lights the whole view equally: the ambient light
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

		Light2DComponent() = default;
		Light2DComponent(const Light2DComponent&) = default;
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
		int32_t SortOrder = 0;                    // sorted with sprites (higher = on top)

		// Light per emitter (Roadmap V4, optional; 0 radius = none)
		float LightRadius = 0.0f;
		float LightIntensity = 1.0f;
		uint32_t MaxLights = 0;                   // the youngest particles that get a light

		// One-shot effects: destroy the entity once it stopped emitting and no particles are left
		bool DestroyWhenFinished = false;

		ParticleEmitter2DComponent() = default;
		ParticleEmitter2DComponent(const ParticleEmitter2DComponent&) = default;
	};

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using CopyableComponents = ComponentGroup<TransformComponent, SpriteRendererComponent, CameraComponent, SpriteAnimatorComponent, Light2DComponent, Shadow2DComponent,
		Rigidbody2DComponent, BoxCollider2DComponent, CircleCollider2DComponent, ParticleEmitter2DComponent, MeshComponent>;
}
