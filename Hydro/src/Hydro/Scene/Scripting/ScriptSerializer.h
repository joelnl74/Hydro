// Hydro/Scene/Scripting/ScriptSerializer.h
#pragma once

#include "Hydro/Scene/Scripting/ScriptableEntity.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <string>

namespace Hydro
{
	// An entity's scripts in a scene or prefab file (`Scripts:` with Name and Properties). Former names are read into
	// the current script or property and logged; values no property reads are kept and written back (#143).
	class ScriptSerializer
	{
	public:
		// The Scripts sequence. `owner` names the entity in log messages.
		static void Write(YAML::Emitter& out, const NativeScriptComponent& scripts, const std::string& owner);

		// Binds the scripts of a Scripts sequence on `scripts`, with their saved values.
		static void Read(const YAML::Node& scriptsNode, NativeScriptComponent& scripts, const std::string& owner);

		// One property value as YAML: vectors as [x, y, z], assets as handles, rows as { Table, Row }, data values as a
		// data file has them. Read falls back to the property's default for a value of another kind.
		static void WriteValue(YAML::Emitter& out, const ScriptPropertyValue& value);
		static ScriptPropertyValue ReadValue(const YAML::Node& node, const ScriptProperty& property);
	};
}
