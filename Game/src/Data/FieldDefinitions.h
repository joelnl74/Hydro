// Game/src/Data/FieldDefinitions.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <string>
#include <vector>

// What the field's people and signs say (FieldDirectorScript). The field scenes find them by their tags: the row ID is
// the name after "Npc " / "Sign " without spaces ("Npc Village Elder" -> VillageElder, "Sign Shrine" -> Shrine), so
// the scenes didn't have to change. Assets/Data/Npcs.hytable and Signs.hytable.
namespace Hydro
{
	HY_DATA(Table, DisplayField(Name))
	struct NpcDefinition
	{
		HY_PROPERTY(Tooltip("The name on the dialogue box (when the dialogue has no speaker of its own)"))
		std::string Name;
		HY_PROPERTY(Asset(Definition, DialogueDefinition), Tooltip("What they say when the player talks to them"))
		AssetHandle Dialogue = 0;
	};

	HY_DATA(Table)
	struct SignDefinition
	{
		HY_PROPERTY(Tooltip("The lines shown in the message box (two fit)"))
		std::vector<std::string> Lines;
	};
}
