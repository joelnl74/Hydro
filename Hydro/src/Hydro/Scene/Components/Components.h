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
#include <glm/gtx/quaternion.hpp>

namespace Hydro
{
	class Entity;
	class Scene;
	class UUID;

	// A quad drawn with the entity's TransformComponent: Texture (white when 0 or not loaded) multiplied by Color
	// (alpha blended). A Single texture is drawn whole, repeated TilingFactor times; a Multiple texture
	// (a sprite sheet, see TextureImportConfig) draws its sprite SpriteIndex, placed by the sprite's pivot.
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
		// Equal values: lower Translation.z first, then the order the scene stores them in.
		int32_t SortOrder = 0;
		// Which of the texture's sprites to draw (Texture2D::GetSprite). 0 is the whole image of a
		// Single texture and the first sprite of a Multiple one; out of range draws sprite 0.
		// A playing SpriteAnimatorComponent sets it every frame.
		uint32_t SpriteIndex = 0;
		bool FlipX = false;
		bool FlipY = false;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {
		}
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

		// Runtime state: not saved, written only by UpdateSpriteAnimations and SpriteAnimator.
		int32_t ClipIndex = -1; // -1 = nothing started yet
		uint32_t FrameIndex = 0;
		float FrameTime = 0.0f; // seconds into the current frame
		int8_t Step = 1;        // PingPong direction: 1 forwards, -1 backwards
		bool Playing = false;
		bool Finished = false;  // a Once clip reached its last frame

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

		Light2DComponent() = default;
		Light2DComponent(const Light2DComponent&) = default;
	};

	// Every engine component that is plain, copyable data, apart from TagComponent and IDComponent
	// (every entity has those). Scene::Copy copies these; add new copyable components here.
	template<typename... Component>
	struct ComponentGroup
	{
	};

	using CopyableComponents = ComponentGroup<TransformComponent, SpriteRendererComponent, CameraComponent, SpriteAnimatorComponent, Light2DComponent>;
}
