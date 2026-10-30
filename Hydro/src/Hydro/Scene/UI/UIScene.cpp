#include "hypch.h"
#include "UIScene.h"

#include "Hydro/Core/Input.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SystemContext.h"
#include "Hydro/Scene/UI/UI.h"
#include "Hydro/Scene/UI/UILayout.h"

#include <algorithm>

#include <cmath>
#include <limits>

namespace Hydro
{
	// The canvas a resolved element is drawn on (0 = none).
	static uint64_t GetCanvasID(Entity element)
	{
		return element.HasComponent<UIResolvedRectComponent>() ? (uint64_t)element.GetComponent<UIResolvedRectComponent>().Canvas : 0;
	}

	static glm::vec2 GetCentre(Entity element)
	{
		const UIResolvedRectComponent& rect = element.GetComponent<UIResolvedRectComponent>();
		return (rect.Min + rect.Max) * 0.5f;
	}

	// A button that can take the focus and be pressed: interactable and drawn.
	static bool IsUsable(Entity button)
	{
		return button.IsValid() && button.HasComponent<UIButtonComponent>() && button.HasComponent<UIResolvedRectComponent>()
			&& button.GetComponent<UIResolvedRectComponent>().Visible;
	}

	void UIScene::Focus(Scene& scene, Entity button)
	{
		if (button.IsValid() == false || button.HasComponent<UIButtonComponent>() == false)
		{
			HY_CORE_WARN("UI: only buttons take the focus ('{}' has no UI Button)", button.IsValid() ? button.GetName() : std::string("?"));
			return;
		}
		SetFocus(scene, button, m_Pending);
	}

	void UIScene::ClearFocus(Scene& scene)
	{
		SetFocus(scene, {}, m_Pending);
	}

	Entity UIScene::GetFocused(Scene& scene) const
	{
		if ((uint64_t)m_Focused == 0)
		{
			return {};
		}
		Entity focused = scene.GetEntityByUUID(m_Focused);
		return focused.IsValid() ? focused : Entity{};
	}

	UIInteractionState UIScene::GetState([[maybe_unused]] Scene& scene, Entity button) const
	{
		if (button.IsValid() == false || button.HasComponent<UIButtonComponent>() == false)
		{
			return UIInteractionState::Normal;
		}
		if (button.GetComponent<UIButtonComponent>().Interactable == false)
		{
			return UIInteractionState::Disabled;
		}
		const uint64_t id = (uint64_t)button.GetUUID();
		if ((uint64_t)m_Pressed == id && (uint64_t)m_Hovered == id)
		{
			return UIInteractionState::Pressed;
		}
		if ((uint64_t)m_Focused == id)
		{
			return UIInteractionState::Focused;
		}
		return UIInteractionState::Normal;
	}

	Entity UIScene::HitTest(Scene& scene, const glm::vec2& viewPosition) const
	{
		// The canvases that take the pointer, topmost first: the capturing one alone while it is shown (it is modal),
		// else every visible top-level canvas shown since the last update, by SortOrder and then hierarchy order.
		struct Candidate
		{
			int32_t SortOrder = 0;
			uint32_t Index = 0;
			Entity Canvas;
		};
		std::vector<Candidate> canvases;
		const Entity capturing = FindTopCapturingCanvas(scene);
		if ((bool)capturing)
		{
			canvases.push_back({ 0, 0, capturing });
		}
		else
		{
			const std::vector<entt::entity>& order = scene.GetEntityOrder();
			for (uint32_t i = 0; i < (uint32_t)order.size(); i++)
			{
				if (scene.m_Registry.has<UICanvasComponent>(order[i]) == false)
				{
					continue;
				}
				Entity canvas{ order[i], &scene };
				const UICanvasComponent& component = canvas.GetComponent<UICanvasComponent>();
				bool nested = false;
				UI::FindCanvas(canvas, nested);
				if (component.Visible && nested == false && m_ShownCanvases.count((uint64_t)canvas.GetUUID()) != 0)
				{
					canvases.push_back({ component.SortOrder, i, canvas });
				}
			}
			std::sort(canvases.begin(), canvases.end(), [](const Candidate& a, const Candidate& b)
			{
				return a.SortOrder != b.SortOrder ? a.SortOrder > b.SortOrder : a.Index > b.Index;
			});
		}

		std::vector<Entity> subtree;
		for (const Candidate& candidate : canvases)
		{
			// Where the last layout put the canvas (a World canvas follows its entity); not placed: not under the pointer.
			UICanvasMetrics metrics;
			if (scene.GetUICanvasMetrics(candidate.Canvas.GetUUID(), metrics) == false)
			{
				continue;
			}
			const glm::vec2 point = (viewPosition - metrics.Offset) / metrics.Scale; // canvas pixels
			const uint64_t canvasID = (uint64_t)candidate.Canvas.GetUUID();

			// Later in the hierarchy is drawn on top: walk it backwards.
			scene.GetSubtree(candidate.Canvas, subtree);
			for (auto it = subtree.rbegin(); it != subtree.rend(); ++it)
			{
				Entity element = *it;
				if (element.HasComponent<UIButtonComponent>() == false || element.GetComponent<UIButtonComponent>().Interactable == false
					|| element.HasComponent<UIResolvedRectComponent>() == false)
				{
					continue;
				}
				const UIResolvedRectComponent& rect = element.GetComponent<UIResolvedRectComponent>();
				// Inside its rect and inside what the scroll views above it show.
				const glm::vec2 min = glm::max(rect.Min, rect.ClipMin);
				const glm::vec2 max = glm::min(rect.Max, rect.ClipMax);
				if (rect.Visible && (uint64_t)rect.Canvas == canvasID && point.x >= min.x && point.x < max.x
					&& point.y >= min.y && point.y < max.y)
				{
					return element;
				}
			}
		}
		return {};
	}

	bool UIScene::UpdatePointer(Scene& scene, std::vector<UIEvent>& events)
	{
		const Scene::UIPointer& pointer = scene.GetUIPointer();
		const Entity hit = pointer.Inside ? HitTest(scene, pointer.Position) : Entity{};
		m_Hovered = (bool)hit ? hit.GetUUID() : UUID(0);

		// Real movement (not a pointer resting where it was) or a click hands the focus to the pointer.
		const bool moved = m_PointerKnown && pointer.Inside && glm::length(pointer.Position - m_PointerPosition) > 1.0f;
		const bool pressedNow = pointer.Down && m_PointerDown == false && pointer.Inside;
		const bool releasedNow = pointer.Down == false && m_PointerDown;
		if (moved || pressedNow)
		{
			m_Modality = UIInputModality::Pointer;
		}

		// A press on a button that has gone (destroyed, hidden, disabled) is over.
		Entity pressed = (uint64_t)m_Pressed != 0 ? scene.GetEntityByUUID(m_Pressed) : Entity{};
		if ((uint64_t)m_Pressed != 0 && (IsUsable(pressed) == false || pressed.GetComponent<UIButtonComponent>().Interactable == false))
		{
			m_Pressed = 0;
			pressed = {};
		}

		if (pressedNow && (bool)hit)
		{
			m_Pressed = hit.GetUUID();
			pressed = hit;
			SetFocus(scene, hit, events);
		}
		else if (moved && (bool)hit && (uint64_t)m_Pressed == 0)
		{
			SetFocus(scene, hit, events);
		}

		if (releasedNow && (bool)pressed)
		{
			// Released over the button it started on: Submit, once. Anywhere else: the press is dropped quietly.
			if (hit == pressed)
			{
				events.push_back({ UIEventType::Submit, pressed, pressed.GetComponent<UIButtonComponent>().Event });
			}
			m_Pressed = 0;
		}

		m_PointerDown = pointer.Down;
		m_PointerPosition = pointer.Position;
		m_PointerKnown = pointer.Inside;
		return (uint64_t)m_Pressed != 0;
	}

	void UIScene::SetFocus(Scene& scene, Entity button, std::vector<UIEvent>& events)
	{
		const Entity previous = GetFocused(scene);
		if (previous == button)
		{
			return;
		}
		if ((bool)previous)
		{
			events.push_back({ UIEventType::FocusExit, previous, std::string() });
		}
		m_Focused = (bool)button ? button.GetUUID() : UUID(0);
		if ((bool)button)
		{
			events.push_back({ UIEventType::FocusEnter, button, std::string() });
		}
	}

	Entity UIScene::FindNext(Scene& scene, Entity from, Action action) const
	{
		const UIButtonComponent& button = from.GetComponent<UIButtonComponent>();
		if (button.Navigation == UINavigation::None)
		{
			return {};
		}
		if (button.Navigation == UINavigation::Explicit)
		{
			UUID target = 0;
			switch (action)
			{
			case ActionUp: target = button.Up; break;
			case ActionDown: target = button.Down; break;
			case ActionLeft: target = button.Left; break;
			case ActionRight: target = button.Right; break;
			default: break;
			}
			Entity next = (uint64_t)target != 0 ? scene.GetEntityByUUID(target) : Entity{};
			return IsUsable(next) && next.GetComponent<UIButtonComponent>().Interactable ? next : Entity{};
		}

		// Auto: the nearest interactable button on the same canvas whose centre lies that way; buttons in line with
		// this one count as nearer than ones off to the side.
		static const glm::vec2 s_Directions[4] = { { 0.0f, 1.0f }, { 0.0f, -1.0f }, { -1.0f, 0.0f }, { 1.0f, 0.0f } };
		const glm::vec2 direction = s_Directions[(uint32_t)action];
		const glm::vec2 origin = GetCentre(from);
		const uint64_t canvas = GetCanvasID(from);

		Entity best;
		float bestScore = std::numeric_limits<float>::max();
		auto view = scene.m_Registry.view<UIButtonComponent, UIResolvedRectComponent>();
		for (entt::entity handle : view)
		{
			Entity candidate{ handle, &scene };
			if (candidate == from || view.get<UIButtonComponent>(handle).Interactable == false)
			{
				continue;
			}
			const UIResolvedRectComponent& rect = view.get<UIResolvedRectComponent>(handle);
			if (rect.Visible == false || (uint64_t)rect.Canvas != canvas)
			{
				continue;
			}
			const glm::vec2 offset = (rect.Min + rect.Max) * 0.5f - origin;
			const float along = glm::dot(offset, direction);
			if (along <= 0.5f)
			{
				continue;
			}
			const float across = std::abs(offset.x * direction.y - offset.y * direction.x);
			const float score = along + across * 2.0f;
			if (score < bestScore)
			{
				bestScore = score;
				best = candidate;
			}
		}
		return best;
	}

	Entity UIScene::FindTopCapturingCanvas(Scene& scene) const
	{
		Entity top;
		int32_t topOrder = std::numeric_limits<int32_t>::min();
		// In list order, so of equal sort orders the later one (drawn on top) wins.
		for (entt::entity handle : scene.GetEntityOrder())
		{
			if (scene.m_Registry.has<UICanvasComponent>(handle) == false)
			{
				continue;
			}
			const UICanvasComponent& canvas = scene.m_Registry.get<UICanvasComponent>(handle);
			Entity entity{ handle, &scene };
			if (canvas.Visible == false || canvas.BlocksGameplayInput == false || m_ShownCanvases.count((uint64_t)entity.GetUUID()) == 0)
			{
				continue;
			}
			bool nested = false;
			UI::FindCanvas(entity, nested);
			if (nested == false && canvas.SortOrder >= topOrder)
			{
				top = entity;
				topOrder = canvas.SortOrder;
			}
		}
		return top;
	}

	void UIScene::Update(Scene& scene, const SystemContext& context, std::vector<UIEvent>& events)
	{
		// Focus changes asked for since the last update go out first.
		for (UIEvent& event : m_Pending)
		{
			events.push_back(std::move(event));
		}
		m_Pending.clear();

		// The keys: which went down this frame, and the held direction's repeat.
		const bool down[ActionCount] = {
			Input::IsKeyPressed(KeyCode::Up) || Input::IsKeyPressed(KeyCode::W),
			Input::IsKeyPressed(KeyCode::Down) || Input::IsKeyPressed(KeyCode::S),
			Input::IsKeyPressed(KeyCode::Left) || Input::IsKeyPressed(KeyCode::A),
			Input::IsKeyPressed(KeyCode::Right) || Input::IsKeyPressed(KeyCode::D),
			Input::IsKeyPressed(KeyCode::Enter) || Input::IsKeyPressed(KeyCode::KPEnter) || Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Z),
			Input::IsKeyPressed(KeyCode::Escape) || Input::IsKeyPressed(KeyCode::Backspace) || Input::IsKeyPressed(KeyCode::X) };
		bool pressed[ActionCount] = {};
		bool anyPressed = false;
		for (uint32_t i = 0; i < ActionCount; i++)
		{
			pressed[i] = down[i] && m_Down[i] == false;
			anyPressed |= pressed[i];
			m_Down[i] = down[i];
		}

		int32_t move = -1;
		for (uint32_t i = ActionUp; i <= ActionRight; i++)
		{
			if (pressed[i])
			{
				move = (int32_t)i;
				m_RepeatAction = (int32_t)i;
				m_RepeatTime = 0.0f;
			}
		}
		if (move < 0 && m_RepeatAction >= 0)
		{
			if (down[m_RepeatAction])
			{
				m_RepeatTime += context.DeltaTime;
				if (m_RepeatTime >= RepeatDelay)
				{
					move = m_RepeatAction;
					m_RepeatTime = RepeatDelay - RepeatInterval;
				}
			}
			else
			{
				m_RepeatAction = -1;
			}
		}

		// A focused button that was destroyed, hidden (its canvas too) or left its canvas loses the focus.
		Entity focused = GetFocused(scene);
		if ((uint64_t)m_Focused != 0 && IsUsable(focused) == false)
		{
			SetFocus(scene, {}, events);
			focused = {};
		}

		// The pointer first: hover, press and release can move the focus and send Submit.
		const bool pointerHeld = UpdatePointer(scene, events);
		focused = GetFocused(scene);

		// UI keys only while something is focused on a canvas shown since the last update, or a capturing canvas is.
		// The topmost capturing canvas is modal: a focus left on another canvas behind it takes no keys meanwhile.
		const Entity capturing = FindTopCapturingCanvas(scene);
		bool focusUsable = (bool)focused && m_ShownCanvases.count(GetCanvasID(focused)) != 0;
		if (focusUsable && (bool)capturing && GetCanvasID(focused) != (uint64_t)capturing.GetUUID())
		{
			focusUsable = false;
		}
		if (focusUsable || (bool)capturing)
		{
			if (anyPressed)
			{
				m_Modality = UIInputModality::Keyboard;
			}
			if (move >= 0 && focusUsable)
			{
				if (Entity next = FindNext(scene, focused, (Action)move))
				{
					SetFocus(scene, next, events);
					focused = next;
				}
			}
			// A Submit key while a pointer press is held is ignored, so one action can't submit twice.
			if (pressed[ActionSubmit] && pointerHeld == false)
			{
				if (focusUsable)
				{
					const UIButtonComponent& button = focused.GetComponent<UIButtonComponent>();
					if (button.Interactable)
					{
						events.push_back({ UIEventType::Submit, focused, button.Event });
					}
				}
				else
				{
					events.push_back({ UIEventType::Submit, capturing, std::string() });
				}
			}
			if (pressed[ActionCancel])
			{
				events.push_back({ UIEventType::Cancel, focusUsable ? focused : capturing, std::string() });
			}
		}

		// Which canvases are shown now: the next update takes their input.
		m_ShownCanvases.clear();
		auto canvases = scene.m_Registry.view<UICanvasComponent, IDComponent>();
		for (entt::entity handle : canvases)
		{
			if (canvases.get<UICanvasComponent>(handle).Visible)
			{
				m_ShownCanvases.insert((uint64_t)canvases.get<IDComponent>(handle).Id);
			}
		}
	}
}
