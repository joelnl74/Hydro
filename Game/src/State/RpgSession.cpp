#include "State/RpgSession.h"

#include "Scripts/ShrineScripts.h"

namespace Hydro
{
	// The run state types reach the running session through these until SC8 moves them to State/.

	BattleState& BattleState::Get()
	{
		return RpgSession::Get().Battle;
	}

	QuestLog& QuestLog::Get()
	{
		return RpgSession::Get().Quests;
	}

	Inventory& Inventory::Get()
	{
		return RpgSession::Get().Items;
	}

	int& DioramaDirectorScript::LastPreset()
	{
		return RpgSession::Get().LastLightingPreset;
	}
}
