#pragma once

#include "Views/ViewHelpers.h"

#include <string>

namespace Hydro
{
	// The field's message box: a line or two shown for a while, fading out over its last 0.3 s.
	class MessageBannerView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Panel = ViewHelpers::Find(scene, "Field Message Panel", "Message banner");
			m_Text = ViewHelpers::Find(scene, "Field Message Text", "Message banner");
			m_Time = 0.0f;
			Present();
		}

		void Show(const std::string& text, float seconds)
		{
			m_Words = text;
			m_Time = seconds;
			Present();
		}

		void Update(float deltaTime)
		{
			if (m_Time > 0.0f)
			{
				m_Time = std::max(0.0f, m_Time - deltaTime);
				Present();
			}
		}

	private:
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const bool shown = m_Time > 0.0f;
			const float k = shown ? std::min(1.0f, m_Time / 0.3f) : 0.0f;
			Entity panel{ m_Panel, m_Scene };
			ViewHelpers::SetVisible(panel, shown);
			if (shown == false)
			{
				return;
			}
			Entity text{ m_Text, m_Scene };
			UI::SetText(text, m_Words);
			ViewHelpers::SetAlpha(panel, 0.92f * k);
			ViewHelpers::SetAlpha(text, k);
		}

		Scene* m_Scene = nullptr;
		entt::entity m_Panel = entt::null;
		entt::entity m_Text = entt::null;
		std::string m_Words;
		float m_Time = 0.0f;
	};
}
