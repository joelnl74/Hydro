#pragma once

#include "Hydro.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Test bed checks for the runtime UI's keyboard and pointer steps, in the Battle scene (docs/TestBed.md, "Runtime UI").

namespace Hydro
{
	// On "UI Grid Canvas": U shows a 3 x 3 button grid; the status line says what the canvas heard from its buttons. "Off"
	// is not interactable; "Msg" opens "UI Message Canvas" on top.
	HY_SCRIPT()
	class UIGridCheckScript : public ScriptableEntity
	{
	protected:
		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			const bool down = Input::IsKeyPressed(KeyCode::U);
			const bool pressed = down && m_UDown == false;
			m_UDown = down;
			if (pressed)
			{
				Show(UI::IsVisible(GetEntity()) == false);
			}
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			Scene& scene = *GetEntity().GetScene();
			switch (event.Type)
			{
			case UIEventType::Submit:
				if (event.Name == "Message")
				{
					UI::SetVisible(Find(scene, {}, "UI Message Canvas"), true);
				}
				SetStatus("Submit: " + (event.Name.empty() ? std::string("(canvas)") : event.Name));
				return true;
			case UIEventType::Cancel:
				Show(false);
				return true;
			case UIEventType::FocusEnter:
				SetStatus("Focus: " + event.Target.GetName());
				return true;
			default:
				return false;
			}
		}

	private:
		// Below `root`, or anywhere in the scene with an empty root.
		static Entity Find(Scene& scene, Entity root, const std::string& name)
		{
			if ((bool)root)
			{
				std::vector<Entity> subtree;
				scene.GetSubtree(root, subtree);
				for (Entity entity : subtree)
				{
					if (entity.GetName() == name)
					{
						return entity;
					}
				}
				return {};
			}
			auto view = scene.m_Registry.view<TagComponent>();
			for (entt::entity handle : view)
			{
				if (view.get<TagComponent>(handle).Tag == name)
				{
					return Entity{ handle, &scene };
				}
			}
			return {};
		}

		void Show(bool show)
		{
			Scene& scene = *GetEntity().GetScene();
			UI::SetVisible(GetEntity(), show);
			if (show)
			{
				UI::Focus(Find(scene, GetEntity(), "Grid Button 1"));
				SetStatus("Arrows move, Enter presses, Esc closes");
			}
		}

		void SetStatus(const std::string& text)
		{
			UI::SetText(Find(*GetEntity().GetScene(), GetEntity(), "Grid Status"), text);
		}

		bool m_UDown = false;
	};

	// On "UI Message Canvas": Enter or Esc closes it. With nothing focused, Submit and Cancel go to the canvas itself.
	HY_SCRIPT()
	class UIMessageCheckScript : public ScriptableEntity
	{
	protected:
		bool OnUIEvent(const UIEvent& event) override
		{
			if (event.Type == UIEventType::Submit || event.Type == UIEventType::Cancel)
			{
				UI::SetVisible(GetEntity(), false);
				return true;
			}
			return false;
		}
	};
}
