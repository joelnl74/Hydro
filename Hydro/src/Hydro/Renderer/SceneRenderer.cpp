#include "hypch.h"

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <algorithm>
#include <cmath>

namespace Hydro
{
	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera, const EditorOverlay& overlay)
	{
		Renderer2D::BeginScene(view, camera.GetViewProjection());
		SubmitSprites();
		SubmitLights();
		SubmitShadows();
		SubmitUnlitParticles();
		SubmitEditorOverlay(overlay);
		Renderer2D::EndScene();
	}

	bool SceneRenderer::RenderRuntime(RenderView& view)
	{
		Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
		if ((bool)cameraEntity == false)
		{
			// Nothing to look through: begin the view anyway so it is cleared, not left stale.
			Renderer2D::BeginScene(view, glm::mat4(1.0f));
			Renderer2D::EndScene();
			return false;
		}

		// The camera's position and rotation; scale would distort the view, so it's ignored.
		glm::mat4 cameraTransform(1.0f);
		if (cameraEntity.HasComponent<TransformComponent>())
		{
			const TransformComponent& transform = cameraEntity.GetComponent<TransformComponent>();
			cameraTransform = glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(glm::quat(transform.Rotation));
		}

		const SceneCamera& sceneCamera = cameraEntity.GetComponent<CameraComponent>().Camera;
		Renderer2D::BeginScene(view, Camera(sceneCamera.GetProjection(m_Scene->GetViewportAspectRatio())), cameraTransform);
		SubmitSprites();
		SubmitLights();
		SubmitShadows();
		SubmitUnlitParticles();
		Renderer2D::EndScene();
		return true;
	}

	void SceneRenderer::SubmitSprites()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, SpriteRendererComponent>();

		const auto drawsBefore = [](const SpriteDrawItem& a, const SpriteDrawItem& b)
		{
			if (a.SortOrder != b.SortOrder)
			{
				return a.SortOrder < b.SortOrder;
			}
			if (a.Z != b.Z)
			{
				return a.Z < b.Z;
			}
			return a.Index < b.Index;
		};

		// Collect sort keys. Most scenes are already in order (all SortOrder 0, same Z), so the sort
		// is skipped unless an item is out of order.
		m_SpriteQueue.clear();
		bool inOrder = true;
		uint32_t index = 0;
		for (auto entity : view)
		{
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);

			SpriteDrawItem item;
			item.SortOrder = sprite.SortOrder;
			item.Z = transform.Translation.z;
			item.Index = index++;
			item.Entity = entity;

			if (inOrder && m_SpriteQueue.empty() == false && drawsBefore(item, m_SpriteQueue.back()))
			{
				inOrder = false;
			}
			m_SpriteQueue.push_back(item);
		}

		// Lit emitters with live particles join the same order (only while the game runs: there is no
		// ParticleScene while editing).
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles != nullptr)
		{
			auto emitters = m_Scene->m_Registry.view<TransformComponent, ParticleEmitter2DComponent>();
			for (auto entity : emitters)
			{
				const auto [transform, emitter] = emitters.get<TransformComponent, ParticleEmitter2DComponent>(entity);
				const ParticleScene::EmitterRuntime* runtime = particles->Find(entity);
				if (emitter.Lighting == ParticleLighting::Unlit || runtime == nullptr || runtime->Particles.Count == 0)
				{
					continue;
				}

				SpriteDrawItem item;
				item.SortOrder = emitter.SortOrder;
				item.Z = transform.Translation.z;
				item.Index = index++;
				item.Entity = entity;
				item.IsEmitter = true;

				if (inOrder && m_SpriteQueue.empty() == false && drawsBefore(item, m_SpriteQueue.back()))
				{
					inOrder = false;
				}
				m_SpriteQueue.push_back(item);
			}
		}

		if (inOrder == false)
		{
			std::sort(m_SpriteQueue.begin(), m_SpriteQueue.end(), drawsBefore);
		}

		for (const SpriteDrawItem& item : m_SpriteQueue)
		{
			if (item.IsEmitter)
			{
				const ParticleEmitter2DComponent& emitter = m_Scene->m_Registry.get<ParticleEmitter2DComponent>(item.Entity);
				SubmitParticles(emitter, *particles->Find(item.Entity), item.Z);
				continue;
			}
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(item.Entity);
			SubmitSprite(transform.GetTransform(), sprite);
		}

		// TODO MESH RENDERING
	}

	void SceneRenderer::SubmitUnlitParticles()
	{
		const ParticleScene* particles = m_Scene->GetParticleScene();
		if (particles == nullptr)
		{
			return;
		}

		bool began = false;
		auto emitters = m_Scene->m_Registry.view<TransformComponent, ParticleEmitter2DComponent>();
		for (auto entity : emitters)
		{
			const auto [transform, emitter] = emitters.get<TransformComponent, ParticleEmitter2DComponent>(entity);
			const ParticleScene::EmitterRuntime* runtime = particles->Find(entity);
			if (emitter.Lighting != ParticleLighting::Unlit || runtime == nullptr || runtime->Particles.Count == 0)
			{
				continue;
			}
			if (began == false)
			{
				Renderer2D::BeginOverlay();
				began = true;
			}
			SubmitParticles(emitter, *runtime, transform.Translation.z);
		}
		if (began)
		{
			Renderer2D::EndOverlay();
		}
	}

	void SceneRenderer::SubmitParticles(const ParticleEmitter2DComponent& emitter, const ParticleScene::EmitterRuntime& runtime, float z)
	{
		// The emitter's texture when it is loaded (whoever assigned it loaded it), else the soft dot.
		// TODO(V4): ParticleBlend::Additive needs the premultiplied quad pipeline; until then it draws alpha.
		const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(emitter.Texture);
		const Ref<Texture2D>& texture = loaded != nullptr ? loaded : Renderer2D::GetSoftDotTexture();
		TextureRect rect;
		if (loaded != nullptr && loaded->GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = loaded->GetSpriteUVRect(emitter.SpriteIndex);
		}

		const ParticleScene::ParticleBuffer& buffer = runtime.Particles;
		for (uint32_t i = 0; i < buffer.Count; i++)
		{
			const float t = std::clamp(buffer.Age[i] / buffer.Lifetime[i], 0.0f, 1.0f);
			const glm::vec4 color = emitter.StartColor + (emitter.EndColor - emitter.StartColor) * t;
			const float size = buffer.StartSize[i] * (1.0f + (emitter.EndSizeScale - 1.0f) * t);
			Renderer2D::DrawQuad(glm::vec3(buffer.Position[i], z), glm::vec2(size), buffer.Rotation[i], texture, rect, color);
		}
	}

	void SceneRenderer::SubmitLights()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, Light2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, light] = view.get<TransformComponent, Light2DComponent>(entity);

			if (light.Type == Light2DType::Global)
			{
				Renderer2D::DrawGlobalLight(light.Color, light.Intensity);
			}
			else
			{
				Renderer2D::DrawLight(transform.Translation, light.Radius, light.Color, light.Intensity);
			}
		}
	}

	void SceneRenderer::SubmitShadows()
	{
		auto view = m_Scene->m_Registry.view<TransformComponent, Shadow2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, shadow] = view.get<TransformComponent, Shadow2DComponent>(entity);

			const glm::vec3 position = transform.Translation + glm::vec3(shadow.Offset, 0.0f);
			Renderer2D::DrawShadow(position, shadow.Size, shadow.Strength);
		}
	}

	void SceneRenderer::SubmitSprite(const glm::mat4& transform, const SpriteRendererComponent& sprite)
	{
		// No texture, or one that isn't loaded, draws the white texture, i.e. a plain coloured quad. This
		// never loads from disk: whoever assigned the handle loaded the asset (see the component).
		const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
		if (loaded == nullptr)
		{
			Renderer2D::DrawQuad(transform, nullptr, sprite.TilingFactor, sprite.Color);
			return;
		}

		const Texture2D& texture = *loaded;
		TextureRect rect;
		const glm::mat4 quadTransform = GetSpriteQuadTransform(transform, &texture, sprite.SpriteIndex);
		if (texture.GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = texture.GetSpriteUVRect(sprite.SpriteIndex);
		}
		else
		{
			rect.Max = glm::vec2(sprite.TilingFactor);
		}

		if (sprite.FlipX)
		{
			std::swap(rect.Min.x, rect.Max.x);
		}
		if (sprite.FlipY)
		{
			std::swap(rect.Min.y, rect.Max.y);
		}

		Renderer2D::DrawQuad(quadTransform, loaded, rect, sprite.Color);
	}

	glm::mat4 SceneRenderer::GetSpriteQuadTransform(const glm::mat4& transform, const Texture2D* texture, uint32_t spriteIndex)
	{
		if (texture == nullptr || texture->GetSpriteMode() != SpriteMode::Multiple)
		{
			return transform;
		}

		// The quad is centred on the entity; move it so the sprite's pivot is there instead
		// (a feet pivot keeps a character standing on its position while frames change).
		const glm::vec2 pivot = texture->GetSprite(spriteIndex).Pivot;
		if (pivot == glm::vec2(0.5f))
		{
			return transform;
		}
		return transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.5f - pivot.x, 0.5f - pivot.y, 0.0f));
	}

	void SceneRenderer::SubmitEditorOverlay(const EditorOverlay& overlay)
	{
		Entity selected = overlay.Selected; // a copy: Entity::GetScene is not const
		const bool drawSelected = overlay.SelectionOutline && selected.IsValid()
			&& selected.GetScene() == m_Scene.get() && selected.HasComponent<TransformComponent>();
		if (drawSelected == false && overlay.Colliders == false)
		{
			return;
		}

		Renderer2D::BeginOverlay();

		if (overlay.Colliders)
		{
			const glm::vec4 sensorColor{ 1.0f, 0.9f, 0.2f, 1.0f };
			const glm::vec4 staticColor{ 0.3f, 0.9f, 0.3f, 1.0f };
			const glm::vec4 dynamicColor{ 0.3f, 0.55f, 1.0f, 1.0f };
			const glm::vec4 kinematicColor{ 0.3f, 0.9f, 0.9f, 1.0f };

			const auto colorFor = [&](entt::entity entity, bool sensor)
			{
				if (sensor)
				{
					return sensorColor;
				}
				if (m_Scene->m_Registry.has<Rigidbody2DComponent>(entity) == false)
				{
					return staticColor;
				}
				switch (m_Scene->m_Registry.get<Rigidbody2DComponent>(entity).Type)
				{
				case Rigidbody2DType::Dynamic: return dynamicColor;
				case Rigidbody2DType::Kinematic: return kinematicColor;
				default: return staticColor;
				}
			};

			auto boxes = m_Scene->m_Registry.view<TransformComponent, BoxCollider2DComponent>();
			for (auto entity : boxes)
			{
				const auto [transform, box] = boxes.get<TransformComponent, BoxCollider2DComponent>(entity);
				const glm::mat4 shape = transform.GetTransform()
					* glm::translate(glm::mat4(1.0f), glm::vec3(box.Offset, 0.0f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(box.Size, 1.0f));
				Renderer2D::DrawRect(shape, colorFor(entity, box.IsSensor), 1.5f);
			}

			auto circles = m_Scene->m_Registry.view<TransformComponent, CircleCollider2DComponent>();
			for (auto entity : circles)
			{
				const auto [transform, circle] = circles.get<TransformComponent, CircleCollider2DComponent>(entity);
				const glm::vec3 centre = glm::vec3(transform.GetTransform() * glm::vec4(circle.Offset, 0.0f, 1.0f));
				const float scale = std::max(std::abs(transform.Scale.x), std::abs(transform.Scale.y));
				Renderer2D::DrawCircle(centre, circle.Radius * scale, colorFor(entity, circle.IsSensor), 1.5f);
			}
		}

		if (drawSelected)
		{
			const glm::vec4 selectionColor{ 1.0f, 0.65f, 0.1f, 1.0f };
			const float thickness = 2.5f;
			const TransformComponent& transform = selected.GetComponent<TransformComponent>();
			const glm::mat4 matrix = transform.GetTransform();

			if (selected.HasComponent<SpriteRendererComponent>())
			{
				const SpriteRendererComponent& sprite = selected.GetComponent<SpriteRendererComponent>();
				const Ref<Texture2D> loaded = AssetManager::GetLoadedAsset<Texture2D>(sprite.Texture);
				Renderer2D::DrawRect(GetSpriteQuadTransform(matrix, loaded.get(), sprite.SpriteIndex), selectionColor, thickness);
			}
			else
			{
				// Nothing drawn to outline: a small box on the entity (a point light also shows its reach).
				Renderer2D::DrawRect(transform.Translation, glm::vec2(0.6f), selectionColor, thickness);
				if (selected.HasComponent<Light2DComponent>())
				{
					const Light2DComponent& light = selected.GetComponent<Light2DComponent>();
					if (light.Type == Light2DType::Point)
					{
						Renderer2D::DrawCircle(transform.Translation, light.Radius, selectionColor, 1.5f);
					}
				}
			}
		}

		Renderer2D::EndOverlay();
	}
}
