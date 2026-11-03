#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/ParticleScene.h"
#include "Hydro/Scene/UI/UILayout.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/TextLayout.h"
#include "Hydro/Renderer/RenderFeatures.h"
#include "Hydro/Renderer/Texture2D.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	class Gizmos;

	class Texture2D;

	// Submits a scene to Renderer2D through a camera, into a RenderView.
	class SceneRenderer
	{
	public:
		// Editor-only lines over the scene (never lit): the selection and the colliders.
		struct EditorOverlay
		{
			Entity Selected;                 // outlined when SelectionOutline is on
			bool SelectionOutline = true;
			bool Colliders = false;          // every collider: sensor yellow, static green, dynamic blue, kinematic cyan
			// The Terrain tool's brush: a circle on the terrain where it would paint.
			bool TerrainBrush = false;
			glm::vec3 TerrainBrushCentre{ 0.0f };
			float TerrainBrushRadius = 1.0f;
			glm::vec4 TerrainBrushColor{ 1.0f, 1.0f, 1.0f, 0.9f };
			// The Path brush's points so far (world space), joined by a line; empty when there are none.
			std::vector<glm::vec3> TerrainPath;
			// Off keeps the Editor view sharp. PostProcessing: depth of field, bloom and vignette (exposure and tonemap stay);
			// Fog: the environment's fog.
			bool PostProcessing = false;
			bool Fog = false;
			// What scripts draw with OnDrawGizmos: lines over the scene; Oxygen draws the labels. Not owned.
			const Gizmos* ScriptGizmos = nullptr;
		};

		SceneRenderer(const Ref<Scene>& scene)
			: m_Scene(scene) {
		}

		void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }
		const Ref<Scene>& GetScene() const { return m_Scene; }

		// Editing: the scene seen through the editor's camera.
		void RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay);
		// An overload, not a default argument: g++ can't default a nested type with member initialisers.
		void RenderEditor(RenderView& view, const EditorCamera& camera);
		// The scene through its primary camera, with its UI canvases on top. Keep Scene::OnViewportResize in step
		// with the view's size. Without a primary camera it draws only the UI and returns false.
		bool RenderRuntime(RenderView& view);

		// The scene's render features when it overrides the project's, else RendererSettings::Features.
		RenderFeatureSettings GetEffectiveRenderFeatures() const;

	private:
		// Moves the camera to whole screen pixels of a `viewHeight` view, measured at its focus point (#136).
		// Only the drawn view moves; the camera entity is left as it is.
		static void SnapCameraToPixels(glm::mat4& cameraTransform, const SceneCamera& camera, uint32_t viewHeight);

		// How the camera of the view being drawn is turned, for SpriteFacing. Made once per frame.
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
		static CameraBasis MakeCameraBasis(const glm::mat3& rotation, UprightSpriteMode upright);

		// How sprites cast sun shadows in the view being drawn (PrepareSunShadowCasting).
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

		// Sorts sprites and lit emitters (SortOrder, then flat sprites by Z, then standing ones far to near) and
		// submits them back to front. With shared depth it also picks each one's SpriteDrawMode.
		void SubmitSprites();
		// Whether and how sprites cast sun shadows this frame, from the render features and the main light.
		void PrepareSunShadowCasting();
		// Meshes and terrains (Renderer3D) when the render features draw meshes; warns once per scene otherwise.
		void SubmitMeshes(RenderView& renderView);
		// Called inside SubmitMeshes' Renderer3D scene.
		void SubmitTerrains();
		// Unlit emitters: drawn after the lighting is composited, so night doesn't darken them. They go on top of
		// every sprite.
		void SubmitUnlitParticles();
		// Overlay (HUD) sprites in SortOrder: unlit, no depth test.
		void SubmitOverlaySprites();
		// The runtime UI (#164): every visible canvas in SortOrder, in screen space after the tonemap, in whole view
		// pixels. Runtime only: a screen-space canvas has no place in the Editor view.
		void SubmitUI();
		// One image over a resolved rect (canvas pixels), whole or nine-sliced, cut to clipMin..clipMax (view pixels).
		static void SubmitUIImage(const UIImageComponent& image, const glm::vec2& min, const glm::vec2& max, const UICanvasMetrics& metrics,
			const glm::vec4& tint,
			const glm::vec2& clipMin, const glm::vec2& clipMax);
		// One text laid out in its rect (wrapped, vertically aligned, up to VisibleCharacters). Nothing without a font.
		void SubmitUIText(const UITextComponent& text, const glm::vec2& min, const glm::vec2& max, const UICanvasMetrics& metrics,
			const glm::vec4& tint,
			const glm::vec2& clipMin, const glm::vec2& clipMax);
		// One emitter's live particles, at the emitter's Z; billboards when the camera is tilted.
		static void SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime,
			const glm::vec3& origin, const CameraBasis& camera);
		// A point light's Height moves it up along the camera's up, like a point on an Upright sprite.
		void SubmitLights();
		// The sun, ambient, fog and the point and spot lights, handed to Renderer2D::SetSceneLighting.
		void SubmitForwardLighting();
		// Blob shadows (Shadow2DComponents) in lit views.
		void SubmitShadows();
		// The selection and collider outlines (lines, after lighting).
		void SubmitEditorOverlay(const EditorOverlay& overlay);
		// Depth-tested 3D lines for the selection: a mesh's bounds, a light's reach or cone, the sun's direction.
		void SubmitSelectionDebugLines(Entity selected);
		// 3D colliders and character capsules, depth tested.
		void SubmitColliderDebugLines();
		// The sprite's quad: the entity transform, moved so a sprite sheet's pivot sits on the entity.
		static glm::mat4 GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// The quad turned for its facing: Upright about the middle of its bottom edge, Billboard about its
		// centre; Flat (and any facing under a top-down camera) unchanged.
		static glm::mat4 ApplyFacing(const glm::mat4& quad, SpriteFacing facing, const CameraBasis& camera);
		// One sprite, flipped and turned for its facing. `shadowCaster`: also queue its sun shadow caster.
		void SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite, const CameraBasis& camera,
			bool shadowCaster = false);
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
			bool Additive = false;  // SpriteDrawMode::Additive (AlphaMode Additive, or an additive lit particle emitter)
		};

		Ref<Scene> m_Scene;
		CameraBasis m_Camera;
		bool m_Fog = true; // the scene's fog in this frame (off in the Editor view unless EditorOverlay::Fog)
		// Reused every frame (capacity kept), so sorting allocates nothing once the scene is drawn.
		std::vector<SpriteDrawItem> m_SpriteQueue;
		SunShadowCasting m_SunShadows;
		std::vector<ShadowCasterQuad> m_ShadowCasters; // this frame's, reused
		// SubmitUI's canvases this frame (SortOrder, first place in the hierarchy, entity), reused.
		struct UICanvasItem
		{
			int32_t SortOrder = 0;
			uint32_t Index = 0;
			entt::entity Entity = entt::null;
		};
		std::vector<UICanvasItem> m_UICanvases;
		std::vector<Entity> m_UISubtree; // the canvas being drawn and its descendants, reused
		TextLayoutResult m_TextLayout;   // SubmitUIText's, reused
		// SubmitUI's buttons above the element being drawn, by depth (a stack), reused: their tint (multiplied down)
		// and whether the nearest is focused (UIFocusVisibilityComponent).
		struct UIButtonFrame
		{
			uint32_t Depth = 0;
			glm::vec4 Tint{ 1.0f };
			bool Focused = false;
		};
		std::vector<UIButtonFrame> m_UIButtons;
		bool m_MeshWarningShown = false;
	};
}
