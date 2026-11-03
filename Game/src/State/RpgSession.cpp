#include "State/RpgSession.h"

#include "Scripts/Common/DioramaDirectorScript.h"

namespace Hydro
{
	// Each run state's Get() returns the running session's.

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
