#pragma once
#include "Hydro/Core/UUID.h"
#include "Hydro/Scene/UI/UIEvent.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace Hydro
{
	class Scene;
	struct SystemContext;

	// How a button looks right now (its UIButtonComponent tint).
	enum class UIInteractionState : uint8_t { Normal = 0, Focused, Pressed, Disabled };
	// What moved the focus last. Hover moves the focus only in Pointer modality (set by real movement or a click),
	// so a mouse resting over a button never takes the focus back from the keyboard.
	enum class UIInputModality : uint8_t { Keyboard = 0, Pointer };

	// The running scene's UI state: focus, last frame's keys and the repeat timer. Exists while the game runs; Update
	// is stage 0. Keys and modal canvases: docs/SceneAndECS.md#runtime-ui. Scripts use the UI facade (UI.h).
	class UIInteraction
	{
	public:
		static constexpr float RepeatDelay = 0.35f;
		static constexpr float RepeatInterval = 0.1f;

		// Pointer (Scene::SetUIPointer): moving it switches to Pointer modality. Pressing over a button presses and focuses
		// it; releasing over it sends Submit, releasing elsewhere aborts with no event.

		// Stage 0: reads the keys, moves the focus and appends this frame's events (queued Focus / ClearFocus first).
		// Uses the layout of the last ResolveUILayout.
		void Update(Scene& scene, const SystemContext& context, std::vector<UIEvent>& events);

		// Anything without a UIButtonComponent is ignored with a warning. FocusExit / FocusEnter go out at the next update.
		void Focus(Scene& scene, Entity button);
		void ClearFocus(Scene& scene);
		// The focused button, or an empty Entity.
		Entity GetFocused(Scene& scene) const;
		UIInteractionState GetState(Scene& scene, Entity button) const;
		UIInputModality GetModality() const { return m_Modality; }

		// The interactable button drawn topmost at `viewPosition` (view pixels, Y up) on a canvas shown since the last
		// update; only the capturing canvas's buttons while one is shown.
		Entity HitTest(Scene& scene, const glm::vec2& viewPosition) const;

	private:
		enum Action : uint32_t { ActionUp = 0, ActionDown, ActionLeft, ActionRight, ActionSubmit, ActionCancel, ActionCount };

		// The button focus moves to from `from` in direction `action`, or an empty Entity.
		Entity FindNext(Scene& scene, Entity from, Action action) const;
		// The visible capturing canvas on top that was visible at the last update too, or an empty Entity.
		Entity FindTopCapturingCanvas(Scene& scene) const;
		void SetFocus(Scene& scene, Entity button, std::vector<UIEvent>& events);
		// Hover, press and release for Update. True while a press is held (keyboard Submit is ignored then).
		bool UpdatePointer(Scene& scene, std::vector<UIEvent>& events);

		UUID m_Focused = 0;
		UIInputModality m_Modality = UIInputModality::Keyboard;
		bool m_Down[ActionCount] = {};
		int32_t m_RepeatAction = -1;
		float m_RepeatTime = 0.0f;
		std::vector<UIEvent> m_Pending;            // from Focus / ClearFocus, sent at the next update
		// The pointer at the last update, whether it was down, the button a press started on and the one under it now.
		glm::vec2 m_PointerPosition{ 0.0f, 0.0f };
		bool m_PointerKnown = false;
		bool m_PointerDown = false;
		UUID m_Pressed = 0;
		UUID m_Hovered = 0;
		std::unordered_set<uint64_t> m_ShownCanvases; // canvases (UUIDs) visible at the last update
	};
}
