#pragma once
#include "Hydro/Scene/Entity.h"

#include <string>

namespace Hydro
{
	class Scene;

	// The runtime UI for scripts: plain functions for what touches the scene's UIInteraction (focus) or must stay
	// consistent. Focus only works while the game runs.
	class UI
	{
	public:
		// The entity's UITextComponent text (nothing without one).
		static void SetText(Entity entity, const std::string& text);
		// A canvas's or an element's Visible. Hiding a canvas or an element drops the focus from a button inside it.
		static void SetVisible(Entity entity, bool visible);
		static bool IsVisible(Entity entity);
		static void SetInteractable(Entity button, bool interactable);

		// Gives the focus to a button; FocusExit / FocusEnter go out at the next UI update (stage 0).
		static void Focus(Entity button);
		static void ClearFocus(Scene& scene);
		// The focused button, or an empty Entity.
		static Entity GetFocused(Scene& scene);

		// True while a visible canvas has BlocksGameplayInput: gameplay scripts ignore the keyboard then.
		static bool CapturesInput(Scene& scene);

		// The canvas `entity` is drawn on (itself for a canvas), or empty; `nested` when that canvas is inside another.
		static Entity FindCanvas(Entity entity, bool& nested);
	};
}
