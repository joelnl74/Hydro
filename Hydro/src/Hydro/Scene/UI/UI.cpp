#include "hypch.h"
#include "UI.h"

#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UIScene.h"

namespace Hydro
{
	// The focused button is inside `entity`'s subtree (or is it).
	static bool HoldsFocus(Scene& scene, Entity entity)
	{
		UIScene* ui = scene.GetUIScene();
		if (ui == nullptr)
		{
			return false;
		}
		const Entity focused = ui->GetFocused(scene);
		return (bool)focused && (focused == entity || scene.IsAncestorOf(entity, focused));
	}

	void UI::SetText(Entity entity, const std::string& text)
	{
		if (entity.IsValid() && entity.HasComponent<UITextComponent>())
		{
			entity.GetComponent<UITextComponent>().Text = text;
		}
	}

	void UI::SetVisible(Entity entity, bool visible)
	{
		if (entity.IsValid() == false)
		{
			return;
		}
		if (entity.HasComponent<UICanvasComponent>())
		{
			entity.GetComponent<UICanvasComponent>().Visible = visible;
		}
		if (entity.HasComponent<UIRectComponent>())
		{
			entity.GetComponent<UIRectComponent>().Visible = visible;
		}
		if (visible == false && HoldsFocus(*entity.GetScene(), entity))
		{
			ClearFocus(*entity.GetScene());
		}
	}

	bool UI::IsVisible(Entity entity)
	{
		if (entity.IsValid() == false)
		{
			return false;
		}
		if (entity.HasComponent<UICanvasComponent>())
		{
			return entity.GetComponent<UICanvasComponent>().Visible;
		}
		return entity.HasComponent<UIRectComponent>() && entity.GetComponent<UIRectComponent>().Visible;
	}

	void UI::SetInteractable(Entity button, bool interactable)
	{
		if (button.IsValid() && button.HasComponent<UIButtonComponent>())
		{
			button.GetComponent<UIButtonComponent>().Interactable = interactable;
		}
	}

	void UI::Focus(Entity button)
	{
		if (button.IsValid() == false)
		{
			return;
		}
		if (UIScene* ui = button.GetScene()->GetUIScene())
		{
			ui->Focus(*button.GetScene(), button);
		}
	}

	void UI::ClearFocus(Scene& scene)
	{
		if (UIScene* ui = scene.GetUIScene())
		{
			ui->ClearFocus(scene);
		}
	}

	Entity UI::GetFocused(Scene& scene)
	{
		UIScene* ui = scene.GetUIScene();
		return ui != nullptr ? ui->GetFocused(scene) : Entity{};
	}

	bool UI::CapturesInput(Scene& scene)
	{
		auto canvases = scene.m_Registry.view<UICanvasComponent>();
		for (entt::entity handle : canvases)
		{
			const UICanvasComponent& canvas = canvases.get<UICanvasComponent>(handle);
			if (canvas.Visible == false || canvas.BlocksGameplayInput == false)
			{
				continue;
			}
			bool nested = false;
			FindCanvas(Entity{ handle, &scene }, nested);
			if (nested == false)
			{
				return true;
			}
		}
		return false;
	}

	Entity UI::FindCanvas(Entity entity, bool& nested)
	{
		nested = false;
		if (entity.IsValid() == false)
		{
			return {};
		}
		Scene& scene = *entity.GetScene();
		Entity canvas;
		for (Entity current = entity; (bool)current; current = scene.GetParent(current))
		{
			if (current.HasComponent<UICanvasComponent>())
			{
				if ((bool)canvas)
				{
					nested = true;
					break;
				}
				canvas = current;
			}
		}
		return canvas;
	}
}
