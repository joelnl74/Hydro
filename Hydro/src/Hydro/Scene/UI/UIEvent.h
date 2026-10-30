#pragma once
#include "Hydro/Scene/Entity.h"

#include <cstdint>
#include <string>

namespace Hydro
{
	enum class UIEventType : uint8_t
	{
		Submit = 0, // Enter / Space / Z on the focused button, or on a capturing canvas with nothing focused
		Cancel,     // Esc / Backspace / X: the focused button, else the topmost capturing canvas
		FocusEnter, // the target got the focus
		FocusExit   // the target lost it
	};

	// What the runtime UI tells scripts (ScriptableEntity::OnUIEvent). Delivered to the scripts on Target first,
	// then on each of its ancestors, until one returns true: a menu script on the panel or canvas hears every row.
	struct UIEvent
	{
		UIEventType Type = UIEventType::Submit;
		Entity Target;          // the button, or the canvas when nothing is focused
		std::string Name;       // Submit on a button: its UIButtonComponent::Event ("UseItem", "Attack"); else empty
	};
}
