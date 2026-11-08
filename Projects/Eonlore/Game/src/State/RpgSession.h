#pragma once

#include "Hydro/Scene/GameSession.h"
#include "State/BattleState.h"
#include "State/PartyState.h"
#include "State/QuestLog.h"
#include "State/SaveGame.h"
#include "State/Inventory.h"

namespace Hydro
{
	// Everything one run of the game keeps across its scenes. Made when the game starts and destroyed when it ends, so a new
	// run starts fresh without resetting anything by hand.
	class RpgSession : public GameSession
	{
	public:
		static RpgSession& Get()
		{
			return GameSession::Get<RpgSession>();
		}

		PartyState Party;            // who has joined, their levels, HP and MP, abilities and equipment
		BattleState Battle;          // the battle hand-off, the weaknesses found
		QuestLog Quests;             // the quests, their banners, the NPCs a quest speaks for
		Inventory Items;             // the party's items
		int LastLightingPreset = -1; // the time-of-day preset the last field director applied (-1: none yet)
		SavePlace Arrival;           // where a just-loaded save puts the player (SaveGame::TakeArrival)
	};
}
