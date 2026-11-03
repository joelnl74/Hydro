#pragma once
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/EntityRef.h>
#include <Hydro/Scene/Scripting/ScriptProperty.h>

namespace Hydro
{
	// A script's HY_PROPERTY fields, and the entity picker game components' EntityRef fields share.
	class ScriptFieldUI
	{
	public:
		// One HY_PROPERTY field of a script (#144, #145). `value` must hold the property's type; `owner` is the script's
		// entity, whose scene references are looked up in. True when it changed.
		static bool PropertyField(const ScriptProperty& property, ScriptPropertyValue& value, Entity owner);
		// An EntityRef / ComponentRef<T> value without its label: the scene's entities that fit, or one dropped from the
		// hierarchy. Also draws the entries of a list of entities. True when it changed.
		static bool EntityRefWidget(Entity owner, const ScriptProperty& property, EntityRef& reference);
	};
}
