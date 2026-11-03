#pragma once

#include "Views/ViewHelpers.h"

#include <algorithm>

namespace Hydro
{
	// The cutscene bars, sliding in from off screen. The cutscene says how far.
	class LetterboxView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Canvas = ViewHelpers::Find(scene, "Cinematic Canvas", "Letterbox");
			m_Top = ViewHelpers::Find(scene, "Letterbox Top", "Letterbox");
			m_Bottom = ViewHelpers::Find(scene, "Letterbox Bottom", "Letterbox");
			if (const UIRectComponent* rect = Rect(m_Top))
			{
				m_Height = rect->SizeDelta.y;
			}
		}

		void SetShown(bool shown)
		{
			if (m_Scene != nullptr)
			{
				UI::SetVisible(Entity{ m_Canvas, m_Scene }, shown);
			}
		}

		// 0: the bars off screen, 1: fully in.
		void Set(float amount)
		{
			const float offset = (1.0f - std::clamp(amount, 0.0f, 1.0f)) * m_Height;
			if (UIRectComponent* top = Rect(m_Top))
			{
				top->AnchoredPosition.y = offset;
			}
			if (UIRectComponent* bottom = Rect(m_Bottom))
			{
				bottom->AnchoredPosition.y = -offset;
			}
		}

	private:
		UIRectComponent* Rect(entt::entity entity) const
		{
			if (m_Scene == nullptr)
			{
				return nullptr;
			}
			Entity handle{ entity, m_Scene };
			return handle.IsValid() ? handle.TryGetComponent<UIRectComponent>() : nullptr;
		}

		Scene* m_Scene = nullptr;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Top = entt::null;
		entt::entity m_Bottom = entt::null;
		float m_Height = 140.0f;
	};
}
