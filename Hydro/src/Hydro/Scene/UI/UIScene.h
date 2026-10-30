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
	// What moved the focus last: the keyboard or the pointer. Hover only moves the focus in Pointer modality, which
	// real pointer movement or a click sets; a key press sets Keyboard, so a mouse resting over a button never takes
	// the focus back from the keyboard.
	enum class UIInputModality : uint8_t { Keyboard = 0, Pointer };

	// The running scene's UI state (component state rule 3, like ParticleScene): which button has the focus, the keys
	// held last frame and the repeat timer. Made by Scene::OnRuntimeStart, dropped by OnRuntimeStop; nothing in it is
	// saved or copied. Scene::OnUpdateRuntime runs Update as stage 0, before the scripts, so they see this frame's
	// Submit and Cancel. Scripts reach it through the UI facade (Scene/UI/UI.h). docs/SceneAndECS.md#runtime-ui.
	//
	// Keyboard (until action mapping, C1): arrows or WASD move the focus, Enter / Space / Z submit, Esc / Backspace / X
	// cancel; a held direction repeats after RepeatDelay, every RepeatInterval. Keys count when they go down, not while
	// held. UI keys are only read while something is focused or a capturing canvas (Visible + BlocksGameplayInput) is
	// shown, and a canvas shown since the last update ignores them until the next: the key that opened a menu can't
	// also press its first row. The topmost capturing canvas is modal: while it is shown, a focus on another canvas
	// takes no keys, and with nothing focused on it Submit and Cancel go to the canvas itself (a dialogue without buttons).
	class UIScene
	{
	public:
		static constexpr float RepeatDelay = 0.35f;
		static constexpr float RepeatInterval = 0.1f;

		// Pointer (UI3b, Scene::SetUIPointer): moving the pointer more than one view pixel switches to Pointer modality,
		// and from then on the button under it takes the focus. Pressing over a button presses it (Pressed tint) and
		// focuses it; releasing over the same button sends Submit once; releasing anywhere else aborts the press with no
		// event (not Cancel: that closes menus).

		// Stage 0: reads the keys, moves the focus, makes this frame's events (appended to `events`, with the ones
		// Focus / ClearFocus queued since the last update first). Runs on the layout of the last ResolveUILayout.
		void Update(Scene& scene, const SystemContext& context, std::vector<UIEvent>& events);

		// Gives the focus to a button (an entity with a UIButtonComponent; anything else is ignored with a warning).
		// FocusExit / FocusEnter go out at the next update.
		void Focus(Scene& scene, Entity button);
		void ClearFocus(Scene& scene);
		// The focused button, or an empty Entity.
		Entity GetFocused(Scene& scene) const;
		UIInteractionState GetState(Scene& scene, Entity button) const;
		UIInputModality GetModality() const { return m_Modality; }

		// The interactable button drawn topmost under `viewPosition` (view pixels, Y up), on a canvas shown since the
		// last update; while a capturing canvas is shown only its buttons count. An empty Entity when there is none.
		Entity HitTest(Scene& scene, const glm::vec2& viewPosition) const;

	private:
		enum Action : uint32_t { ActionUp = 0, ActionDown, ActionLeft, ActionRight, ActionSubmit, ActionCancel, ActionCount };

		// The button focus moves to from `from` in direction `action`, or an empty Entity.
		Entity FindNext(Scene& scene, Entity from, Action action) const;
		// The visible capturing canvas on top that was visible at the last update too, or an empty Entity.
		Entity FindTopCapturingCanvas(Scene& scene) const;
		void SetFocus(Scene& scene, Entity button, std::vector<UIEvent>& events);
		// The pointer's part of Update: hover (after real movement), press, release over the same button (Submit) or
		// elsewhere (aborted, no event). True while a press is held (keyboard Submit is ignored then).
		bool UpdatePointer(Scene& scene, std::vector<UIEvent>& events);

		UUID m_Focused = 0;
		UIInputModality m_Modality = UIInputModality::Keyboard;
		bool m_Down[ActionCount] = {};
		int32_t m_RepeatAction = -1;
		float m_RepeatTime = 0.0f;
		std::vector<UIEvent> m_Pending;            // from Focus / ClearFocus, sent at the next update
		// The pointer (UI3b): where it was at the last update, whether it was down, the button a press started on, and
		// the button under it now.
		glm::vec2 m_PointerPosition{ 0.0f, 0.0f };
		bool m_PointerKnown = false;
		bool m_PointerDown = false;
		UUID m_Pressed = 0;
		UUID m_Hovered = 0;
		std::unordered_set<uint64_t> m_ShownCanvases; // canvases (UUIDs) visible at the last update
	};
}
