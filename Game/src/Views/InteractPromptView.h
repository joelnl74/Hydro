// Game/src/Views/InteractPromptView.h
#pragma once

#include "Views/ViewHelpers.h"

#include <string>

namespace Hydro
{
	// The "E  Talk" prompt over the player's head ("Prompt Canvas", a World canvas, and its "Prompt Text"). A view
	// (Scripting plan SC8f): the director picks the thing in reach and where the head is.
	class InteractPromptView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Canvas = ViewHelpers::Find(scene, "Prompt Canvas", "Interact prompt");
			m_Text = ViewHelpers::Find(scene, "Prompt Text", "Interact prompt");
		}

		void Hide()
		{
			if (m_Scene != nullptr)
			{
				ViewHelpers::SetVisible(Entity{ m_Canvas, m_Scene }, false);
			}
		}

		// Shows `text` with the canvas at `position` (world).
		void Show(const std::string& text, const glm::vec3& position)
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			Entity canvas{ m_Canvas, m_Scene };
			ViewHelpers::SetVisible(canvas, true);
			UI::SetText(Entity{ m_Text, m_Scene }, text);
			if (canvas.IsValid() && canvas.HasComponent<TransformComponent>())
			{
				m_Scene->SetWorldPosition(canvas, position);
			}
		}

	private:
		Scene* m_Scene = nullptr;
		entt::entity m_Canvas = entt::null;
		entt::entity m_Text = entt::null;
	};
}
