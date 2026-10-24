#pragma once

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/ParticleScene.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"

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
		void RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay = {});
		// Runtime: the scene seen through its primary CameraComponent, with the scene's viewport
		// aspect ratio (keep Scene::OnViewportResize in step with the view's size). Draws nothing and
		// returns false when the scene has no primary camera; the view is still cleared.
		bool RenderRuntime(RenderView& view);

	private:
		// Sorts the scene's sprites and lit particle emitters together (SortOrder, then Z, then storage
		// order) and submits them back to front. An emitter is one entry: its particles are drawn together.
		void SubmitSprites();
		// Unlit emitters (ParticleLighting::Unlit): drawn after the view's lighting is composited, so they
		// are not darkened at night. They go on top of every sprite (no SortOrder against sprites yet;
		// the emissive path is decided by the Roadmap V4 spike).
		void SubmitUnlitParticles();
		// One emitter's live particles, at the emitter's Z.
		static void SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime, float z);
		// Submits the scene's Light2DComponents (they light the view when RendererSettings::Lighting is on).
		void SubmitLights();
		// Submits the scene's Shadow2DComponents (blob shadows in lit views, RendererSettings::Shadows).
		void SubmitShadows();
		// The editor overlay: selection outline and collider outlines (Renderer2D lines, after lighting).
		void SubmitEditorOverlay(const EditorOverlay& overlay);
		// The sprite's quad: the entity transform, moved so a sprite sheet's pivot sits on the entity.
		static glm::mat4 GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex);
		// One sprite: the whole texture (tiled), or a sprite of a sheet placed by its pivot; flipped as asked.
		static void SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite);

	private:
		struct SpriteDrawItem
		{
			int32_t SortOrder = 0;
			float Z = 0.0f;
			uint32_t Index = 0; // storage order, so equal keys keep a stable order
			entt::entity Entity = entt::null;
			bool IsEmitter = false; // a ParticleEmitter2DComponent instead of a sprite
		};

		Ref<Scene> m_Scene;
		// Reused every frame (capacity kept), so sorting allocates nothing once the scene is drawn.
		std::vector<SpriteDrawItem> m_SpriteQueue;
	};
}
