// Game/src/State/RpgSession.h
#pragma once

#include "Hydro/Scene/GameSession.h"
#include "State/BattleState.h"
#include "State/QuestLog.h"
#include "State/Inventory.h"

namespace Hydro
{
	// This game's session (Scripting plan SC2): everything one run of the game keeps across its scenes. Made when the
	// game starts (Play, or the standalone player) and destroyed when it ends, so a new run starts with full HP, no
	// quests, the starting items and no light carried over: nothing has to be reset by hand.
	//
	// BattleState and QuestLog still live in their scripts' headers; the refactor (SC8) moves them to State/ with
	// the inventory. Until then each has a Get() that returns this session's (RpgSession.cpp).
	class RpgSession : public GameSession
	{
	public:
		static RpgSession& Get()
		{
			return GameSession::Get<RpgSession>();
		}

		BattleState Battle;          // the party's HP and MP, the battle hand-off, the weaknesses found
		QuestLog Quests;             // the quests, their banners, the NPCs a quest speaks for
		Inventory Items;             // the party's items
		int LastLightingPreset = -1; // the day / dusk / night preset the last field director applied (-1: none yet)
	};
}
