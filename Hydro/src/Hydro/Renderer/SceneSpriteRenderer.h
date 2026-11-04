#pragma once

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/ParticleScene.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Texture2D.h"

#include <entt.hpp>
#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	class Scene;

	// How the camera of the view being drawn is turned, for SpriteFacing. Made once per frame.
	struct SceneCameraBasis
	{
		glm::mat3 Rotation{ 1.0f };          // the camera's rotation: what a Billboard quad turns by
		glm::mat3 Upright{ 1.0f };           // yaw and lean without roll: what an Upright quad turns by
		glm::vec2 GroundForward{ 0.0f, 1.0f }; // "into the screen" along the ground (unit length)
		glm::vec3 Position{ 0.0f };          // the camera's position (set by RenderEditor / RenderRuntime)
		glm::vec3 Forward{ 0.0f, 0.0f, -1.0f }; // where it looks (unit length): view-depth sorting measures along it

		static SceneCameraBasis Make(const glm::mat3& rotation, UprightSpriteMode upright);

		// Looking straight down without turning: every facing draws as authored.
		bool IsTopDown() const
		{
			const glm::mat3 difference = Rotation - glm::mat3(1.0f);
			for (int column = 0; column < 3; column++)
			{
				for (int row = 0; row < 3; row++)
				{
					if (std::abs(difference[column][row]) > 1.0e-5f)
					{
						return false;
					}
				}
			}
			return true;
		}

		// Where the camera's view meets the ground (z = 0): the fog and depth of field measure from it.
		// A camera looking level or up measures from itself.
		glm::vec3 GetGroundFocus() const
		{
			if (Forward.z < -0.05f)
			{
				return Position + Forward * (-Position.z / Forward.z);
			}
			return Position;
		}
	};

	// The scene's sprites and particle emitters for SceneRenderer: sorted, turned for their facing, with the sun
	// shadows of standing cut-outs; then the unlit emitters and the overlay (HUD) sprites.
	class SceneSpriteRenderer
	{
	public:
		// Once per frame before the Submit calls: the scene, its render features, the camera, and whether and how
		// sprites cast sun shadows (from the features and the main light).
		void BeginFrame(Scene& scene, const RenderFeatureSettings& features, const SceneCameraBasis& camera);

		// Sorts sprites and lit emitters (SortOrder, then flat sprites by Z, then standing ones far to near) and
		// submits them back to front. With shared depth it also picks each one's SpriteDrawMode.
		void SubmitSprites();
		// Unlit emitters: drawn after the lighting is composited, so night doesn't darken them. They go on top of
		// every sprite.
		void SubmitUnlitParticles();
		// Overlay (HUD) sprites in SortOrder: unlit, no depth test.
		void SubmitOverlaySprites();

		// The sprite's quad: the entity transform, moved so a sprite sheet's pivot sits on the entity.
		static glm::mat4 GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// The quad turned for its facing: Upright about the middle of its bottom edge, Billboard about its
		// centre; Flat (and any facing under a top-down camera) unchanged.
		static glm::mat4 ApplyFacing(const glm::mat4& quad, SpriteFacing facing, const SceneCameraBasis& camera);

	private:
		// How sprites cast sun shadows in the view being drawn.
		struct SunShadowCasting
		{
			bool Sprites = false;                      // cut-out standing sprites cast (the feature is on, the sun casts)
			SpriteShadowMode Mode = SpriteShadowMode::FaceSun;
			glm::mat3 SunUpright{ 1.0f };              // FaceSun: what turns a flat quad to stand facing the sun
		};
		// One sprite's shadow caster, queued and drawn after the sprites so it doesn't split their draw calls.
		struct ShadowCasterQuad
		{
			glm::mat4 Transform{ 1.0f };
			Ref<Texture2D> Texture;
			TextureRect Rect;
			glm::vec4 Color{ 1.0f };
		};
		struct SpriteDrawItem
		{
			int32_t SortOrder = 0;
			float Z = 0.0f;
			uint32_t Index = 0; // storage order, so equal keys keep a stable order
			entt::entity Entity = entt::null;
			bool IsEmitter = false; // a ParticleEmitter2DComponent instead of a sprite
			bool Standing = false;  // Upright, Billboard or particles: sorted by Depth instead of Z
			float Depth = 0.0f;     // along SceneCameraBasis::GroundForward: larger is further away
			float ViewDepth = 0.0f; // along SceneCameraBasis::Forward from the camera (SpriteSortMode::ViewDepth): larger is further
			bool Cutout = false;    // drawn as a cut-out with shared depth (its AlphaMode, or the view's default)
			bool Additive = false;  // SpriteDrawMode::Additive (AlphaMode Additive, or an additive lit particle emitter)
		};

		void PrepareSunShadowCasting();
		// One emitter's live particles, at the emitter's Z; billboards when the camera is tilted.
		static void SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime,
			const glm::vec3& origin, const SceneCameraBasis& camera);
		// One sprite, flipped and turned for its facing. `shadowCaster`: also queue its sun shadow caster.
		void SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite, const SceneCameraBasis& camera,
			bool shadowCaster = false);
		// The texture a sprite draws (null = a plain colour quad) and the part of it, flipped as asked.
		static Ref<Texture2D> GetSpriteImage(const SpriteRendererComponent& sprite, TextureRect& rect);

		// This frame's (BeginFrame).
		Scene* m_Scene = nullptr;
		RenderFeatureSettings m_Features;
		SceneCameraBasis m_Camera;
		SunShadowCasting m_SunShadows;
		// Reused every frame (capacity kept), so sorting allocates nothing once the scene is drawn.
		std::vector<SpriteDrawItem> m_SpriteQueue;
		std::vector<ShadowCasterQuad> m_ShadowCasters;
	};
}
