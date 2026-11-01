// Game/src/Views/ViewHelpers.h
#pragma once

#include "Hydro.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	// Small runtime-UI helpers the views share (Scripting plan SC8f): finding a view's entities by tag (until the UI
	// builder sets references), an element's alpha, a bar's fill.
	class ViewHelpers
	{
	public:
		// The first entity tagged `tag`, or entt::null with a warning naming `owner` (the view looking for it).
		static entt::entity Find(Scene& scene, const std::string& tag, const char* owner)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == tag)
				{
					return entity;
				}
			}
			HY_WARN("{}: the scene has no entity tagged \"{}\"", owner, tag);
			return entt::null;
		}

		// The alpha of a UI element's image and text.
		static void SetAlpha(Entity entity, float alpha)
		{
			if (entity.IsValid() == false)
			{
				return;
			}
			if (UIImageComponent* image = entity.TryGetComponent<UIImageComponent>())
			{
				image->Color.a = alpha;
			}
			if (UITextComponent* text = entity.TryGetComponent<UITextComponent>())
			{
				text->Color.a = alpha;
			}
		}

		// A UI bar's fill, anchored to the bar's left edge, as wide as the fraction.
		static void SetBarFill(Entity fill, float fraction)
		{
			if (UIRectComponent* rect = fill.IsValid() ? fill.TryGetComponent<UIRectComponent>() : nullptr)
			{
				rect->AnchorMin = { 0.0f, 0.0f };
				rect->AnchorMax = { std::clamp(fraction, 0.0f, 1.0f), 1.0f };
				rect->SizeDelta = { 0.0f, 0.0f };
			}
		}

		static void SetVisible(Entity entity, bool visible)
		{
			if (UI::IsVisible(entity) != visible)
			{
				UI::SetVisible(entity, visible);
			}
		}
	};
}
