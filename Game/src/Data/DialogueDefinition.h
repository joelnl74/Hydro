// Game/src/Data/DialogueDefinition.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <string>
#include <vector>

// What someone says: lines shown one after another in the dialogue box (DialogueScript). Stored either as a file of
// its own (an NPC's dialogue: Assets/Data/Dialogue/<Npc>.hydef) or as rows of a table (a quest's conversations:
// Assets/Data/Dialogue/ShrineErrand.hytable), the same type both ways.
namespace Hydro
{
	HY_DATA()
	struct DialogueLine
	{
		HY_PROPERTY(Tooltip("Who says it; empty = the dialogue's speaker"))
		std::string Speaker;
		HY_PROPERTY(Tooltip("The text; wraps in the dialogue box. A quest can fill in {Left} and the like"))
		std::string Text;
	};

	HY_DATA(Definition, Table)
	struct DialogueDefinition
	{
		HY_PROPERTY(Tooltip("The name on the box; empty = whoever opens it (the NPC's name)"))
		std::string Speaker;
		HY_PROPERTY(Asset(Texture2D), Tooltip("Optional portrait (the dialogue box has no portrait yet)"))
		AssetHandle Portrait = 0;
		HY_PROPERTY(Tooltip("Shown in this order; each line can name its own speaker"))
		std::vector<DialogueLine> Lines;
	};
}
