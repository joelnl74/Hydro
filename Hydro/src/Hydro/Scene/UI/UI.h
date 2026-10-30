#pragma once
#include "Hydro/Scene/Entity.h"

#include <string>

namespace Hydro
{
	class Scene;

	// The runtime UI for scripts (like Physics2D and Particles2D): plain functions over entities. Scripts may edit the
	// UI components directly too; these are for what touches the scene's UIScene (focus) or must stay consistent.
	// Focus only works while the game runs (the scene has a UIScene); while editing it does nothing.
	// docs/SceneAndECS.md#runtime-ui.
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

		// True while a visible canvas has BlocksGameplayInput: gameplay scripts (walking, interacting) ignore the
		// keyboard then. The script that owns the menu keeps reading its own keys (Tab to close it).
		static bool CapturesInput(Scene& scene);

		// The canvas `entity` is drawn on (itself for a canvas), or an empty Entity; `nested` when that canvas is inside
		// another one (not drawn, takes no input).
		static Entity FindCanvas(Entity entity, bool& nested);
	};
}
