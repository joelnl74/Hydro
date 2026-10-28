#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/ParticleScene.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Texture2D.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	class Texture2D;

	// Submits a scene to Renderer2D (render submission, step 4 of the frame order), into a RenderView
	// and through a camera.
	class SceneRenderer
	{
	public:
		// Editor-only drawing on top of the scene (lines, never lit): what is selected and where the
		// colliders are. Not drawn by RenderRuntime.
		struct EditorOverlay
		{
			Entity Selected;                 // outlined when SelectionOutline is on
			bool SelectionOutline = true;
			bool Colliders = false;          // every collider: sensor yellow, static green, dynamic blue, kinematic cyan
		};

		SceneRenderer(const Ref<Scene>& scene)
			: m_Scene(scene) {
		}

		void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }
		const Ref<Scene>& GetScene() const { return m_Scene; }

		// Editing: the scene seen through the editor's camera.
		void RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay);
		// Without an overlay (an overload, not a default argument: g++ can't default a nested type with
		// default member initialisers inside the class; MSVC can).
		void RenderEditor(RenderView& view, const EditorCamera& camera);
		// Runtime: the scene seen through its primary CameraComponent, with the scene's viewport
		// aspect ratio (keep Scene::OnViewportResize in step with the view's size). Draws nothing and
		// returns false when the scene has no primary camera; the view is still cleared.
		bool RenderRuntime(RenderView& view);

		// The scene's render features when it overrides the project's, else RendererSettings::Features.
		// RenderEditor and RenderRuntime hand them to the view every frame.
		RenderFeatureSettings GetEffectiveRenderFeatures() const;

	private:
		// How the camera of the view being drawn is turned, for SpriteFacing (Diorama RPG H1). Made once
		// per RenderEditor / RenderRuntime from the camera's rotation.
		struct CameraBasis
		{
			glm::mat3 Rotation{ 1.0f };          // the camera's rotation: what a Billboard quad turns by
			glm::mat3 Upright{ 1.0f };           // yaw and lean without roll: what an Upright quad turns by
			glm::vec2 GroundForward{ 0.0f, 1.0f }; // "into the screen" along the ground (unit length)
			glm::vec3 Position{ 0.0f };          // the camera's position (set by RenderEditor / RenderRuntime)
			glm::vec3 Forward{ 0.0f, 0.0f, -1.0f }; // where it looks (unit length): view-depth sorting measures along it

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
		};
		static CameraBasis MakeCameraBasis(const glm::mat3& rotation);

		// How the sprites of the view being drawn cast sun shadows (R14). Made once per RenderEditor /
		// RenderRuntime by PrepareSunShadowCasting.
		struct SunShadowCasting
		{
			bool Sprites = false;                      // cut-out standing sprites cast (the feature is on, the sun casts)
			SpriteShadowMode Mode = SpriteShadowMode::FaceSun;
			glm::mat3 SunUpright{ 1.0f };              // FaceSun: what turns a flat quad to stand facing the sun
		};
		// One sprite's shadow caster, queued while the sprites are submitted and drawn after them in one block
		// (so the casters don't split the sprites' draw calls).
		struct ShadowCasterQuad
		{
			glm::mat4 Transform{ 1.0f };
			Ref<Texture2D> Texture;
			TextureRect Rect;
			glm::vec4 Color{ 1.0f };
		};

		// Sorts the scene's sprites and lit particle emitters together (SortOrder; then Flat sprites by Z,
		// then the standing ones, Upright, Billboard and emitters, from far to near along the camera's
		// ground direction; then storage order) and submits them back to front. An emitter is one entry:
		// its particles are drawn together. With shared depth it also picks each one's SpriteDrawMode (decal,
		// cut-out or translucent).
		void SubmitSprites();
		// Whether and how sprites cast sun shadows this frame (m_SunShadows), from the render features and the
		// scene's main light.
		void PrepareSunShadowCasting();
		// MeshComponents (Renderer3D), when the render features draw meshes; a warning once per scene otherwise.
		void SubmitMeshes(RenderView& renderView);
		// Unlit emitters (ParticleLighting::Unlit): drawn after the view's lighting is composited, so they
		// are not darkened at night. They go on top of every sprite (no SortOrder against sprites yet;
		// the emissive path is decided by the Roadmap V4 spike).
		void SubmitUnlitParticles();
		// Sprites marked Overlay (HUD): in the overlay pass, after the unlit particles, in SortOrder; unlit, no depth test.
		void SubmitOverlaySprites();
		// One emitter's live particles, at the emitter's Z; billboards when the camera is tilted.
		static void SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime, float z, const CameraBasis& camera);
		// Submits the scene's Light2DComponents (they light the view when RendererSettings::Lighting is on).
		// A point light's Height moves it up along the camera's up, as a point on an Upright sprite moves.
		void SubmitLights();
		// Forward lighting (plan M6): the sun (Scene::GetMainLight), ambient and fog (the scene's environment,
		// else its Global lights) and the point and spot lights, handed to Renderer2D::SetSceneLighting.
		void SubmitForwardLighting();
		// Submits the scene's Shadow2DComponents (blob shadows in lit views, RendererSettings::Shadows).
		void SubmitShadows();
		// The editor overlay: selection outline and collider outlines (Renderer2D lines, after lighting).
		void SubmitEditorOverlay(const EditorOverlay& overlay);
		// Depth-tested 3D lines for the selection (Renderer2D::DrawLine3D, plan M7): a mesh's bounds, a light's
		// reach or cone, the sun's direction.
		void SubmitSelectionDebugLines(Entity selected);
		// The 3D colliders (ColliderShapes) and character capsules, depth tested (Overlays > Collider Outlines).
		void SubmitColliderDebugLines();
		// The sprite's quad: the entity transform, moved so a sprite sheet's pivot sits on the entity.
		static glm::mat4 GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// The quad turned for its facing: Upright about the middle of its bottom edge, Billboard about its
		// centre; Flat (and any facing under a top-down camera) unchanged.
		static glm::mat4 ApplyFacing(const glm::mat4& quad, SpriteFacing facing, const CameraBasis& camera);
		// One sprite: the whole texture (tiled), or a sprite of a sheet placed by its pivot; flipped as asked,
		// turned for its facing. `shadowCaster` (cut-out standing sprites with sun shadows): also queue its
		// shadow caster, as drawn or turned to face the sun, and look its pixels up on that caster.
		void SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite, const CameraBasis& camera, bool shadowCaster = false);
		// The texture a sprite draws (null = a plain colour quad) and the part of it, flipped as asked.
		static Ref<Texture2D> GetSpriteImage(const SpriteRendererComponent& sprite, TextureRect& rect);

	private:
		struct SpriteDrawItem
		{
			int32_t SortOrder = 0;
			float Z = 0.0f;
			uint32_t Index = 0; // storage order, so equal keys keep a stable order
			entt::entity Entity = entt::null;
			bool IsEmitter = false; // a ParticleEmitter2DComponent instead of a sprite
			bool Standing = false;  // Upright, Billboard or particles: sorted by Depth instead of Z
			float Depth = 0.0f;     // along CameraBasis::GroundForward: larger is further away
			float ViewDepth = 0.0f; // along CameraBasis::Forward from the camera (SpriteSortMode::ViewDepth): larger is further away
			bool Cutout = false;    // drawn as a cut-out with shared depth (its AlphaMode, or the view's default)
		};

		Ref<Scene> m_Scene;
		CameraBasis m_Camera;
		// Reused every frame (capacity kept), so sorting allocates nothing once the scene is drawn.
		std::vector<SpriteDrawItem> m_SpriteQueue;
		SunShadowCasting m_SunShadows;
		std::vector<ShadowCasterQuad> m_ShadowCasters; // this frame's, reused
		bool m_MeshWarningShown = false;
	};
}
