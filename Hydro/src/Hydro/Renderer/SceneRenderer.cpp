#include "hypch.h"

#include "SceneRenderer.h"
#include "Renderer2D.h"
#include <glm/ext/matrix_transform.hpp>

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <algorithm>

namespace Hydro
{
	void SceneRenderer::RenderEditor(RenderView& view, const EditorCamera& camera)
	{
		Renderer2D::BeginScene(view, camera.GetViewProjection());
		SubmitSprites();
		SubmitLights();
		SubmitShadows();
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

		if (inOrder == false)
		{
			std::sort(m_SpriteQueue.begin(), m_SpriteQueue.end(), drawsBefore);
		}

		for (const SpriteDrawItem& item : m_SpriteQueue)
		{
			const auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(item.Entity);
			SubmitSprite(transform.GetTransform(), sprite);
		}

		// TODO MESH RENDERING
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
		glm::mat4 quadTransform = transform;
		if (texture.GetSpriteMode() == SpriteMode::Multiple)
		{
			rect = texture.GetSpriteUVRect(sprite.SpriteIndex);

			// The quad is centred on the entity; move it so the sprite's pivot is there instead
			// (a feet pivot keeps a character standing on its position while frames change).
			const glm::vec2 pivot = texture.GetSprite(sprite.SpriteIndex).Pivot;
			if (pivot != glm::vec2(0.5f))
			{
				quadTransform = transform * glm::translate(glm::mat4(1.0f), glm::vec3(0.5f - pivot.x, 0.5f - pivot.y, 0.0f));
			}
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
}
