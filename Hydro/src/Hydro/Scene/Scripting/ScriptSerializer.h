// Hydro/Scene/Scripting/ScriptSerializer.h
#pragma once

#include "Hydro/Scene/Scripting/ScriptableEntity.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <string>

namespace Hydro
{
	// An entity's scripts in a scene or prefab file (EntitySerializer's NativeScriptComponent):
	//
	//     Scripts:
	//       - Name: SpinScript
	//         Properties: { Speed: 3.5 }
	//       - VillageYSortScript        (a plain name: the first format, still read)
	//
	// Former names (HY_SCRIPT(FormerName("...")), HY_PROPERTY(FormerName("..."))): a binding or a value saved under
	// one is read into the script or property it belongs to now and written under its current name. Each use is
	// logged, so the file can be saved and the former name deleted.
	//
	// Saved values no property reads (the script isn't registered, or a property was renamed without a former name
	// or removed) are kept in NativeScriptComponent::Script::KeptProperties and written back, so saving loses
	// nothing (#143).
	class ScriptSerializer
	{
	public:
		// The Scripts sequence. `owner` names the entity in log messages.
		static void Write(YAML::Emitter& out, const NativeScriptComponent& scripts, const std::string& owner);

		// Binds the scripts of a Scripts sequence on `scripts`, with their saved values.
		static void Read(const YAML::Node& scriptsNode, NativeScriptComponent& scripts, const std::string& owner);

		// One property value: scalars and strings as they are, vectors as [x, y, z], an asset as its handle number, a
		// table row as { Table: <handle>, Row: <id> }. Read gives the property's default for a value of another kind.
		static void WriteValue(YAML::Emitter& out, const ScriptPropertyValue& value);
		static ScriptPropertyValue ReadValue(const YAML::Node& node, const ScriptProperty& property);
	};
}
