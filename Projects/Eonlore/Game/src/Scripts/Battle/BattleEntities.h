#pragma once

#include "Hydro.h"

#include <algorithm>
#include <string>

// Small reads and writes on the Battle scene's entities, shared by the stage and the battle's views.

namespace Hydro
{
	class BattleEntities
	{
	public:
		// The first entity with the tag; warns when there is none.
		static entt::entity Find(Scene& scene, const std::string& tag)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == tag)
				{
					return entity;
				}
			}
			HY_WARN("Battle: the scene has no entity tagged \"{}\"", tag);
			return entt::null;
		}

		static bool Alive(Scene& scene, entt::entity entity)
		{
			return entity != entt::null && scene.m_Registry.valid(entity);
		}

		static void SetColor(Scene& scene, entt::entity entity, const glm::vec4& color)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).Color = color;
			}
		}

		static void SetSprite(Scene& scene, entt::entity entity, uint32_t index)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).SpriteIndex = index;
			}
		}

		static void SetShadowStrength(Scene& scene, entt::entity shadow, float strength)
		{
			if (Alive(scene, shadow) && scene.m_Registry.has<Shadow2DComponent>(shadow))
			{
				scene.m_Registry.get<Shadow2DComponent>(shadow).Strength = strength;
			}
		}

		// Anchored to the bar's left edge, as wide as the fraction.
		static void SetFill(Entity fill, float fraction, const glm::vec4& color)
		{
			if (fill.IsValid() == false || fill.HasComponent<UIRectComponent>() == false)
			{
				return;
			}
			UIRectComponent& rect = fill.GetComponent<UIRectComponent>();
			rect.AnchorMin = { 0.0f, 0.0f };
			rect.AnchorMax = { std::clamp(fraction, 0.0f, 1.0f), 1.0f };
			rect.SizeDelta = { 0.0f, 0.0f };
			rect.AnchoredPosition = { 0.0f, 0.0f };
			SetUIColor(fill, color);
		}

		static void SetUIColor(Entity element, const glm::vec4& color)
		{
			if (element.IsValid() == false)
			{
				return;
			}
			if (element.HasComponent<UIImageComponent>())
			{
				element.GetComponent<UIImageComponent>().Color = color;
			}
			if (element.HasComponent<UITextComponent>())
			{
				element.GetComponent<UITextComponent>().Color = color;
			}
		}

		// Shows or hides a UI element only when that changes, so the layout isn't redone every frame.
		static void SetVisible(Entity element, bool visible)
		{
			if (UI::IsVisible(element) != visible)
			{
				UI::SetVisible(element, visible);
			}
		}
	};
}
