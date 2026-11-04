#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "State/BattleData.h"
#include "State/PartyState.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// The battle hand-off (BattleState, on the game session): what the field and the battle tell each other. The party is
// State/PartyState.h, the numbers from data State/BattleData.h.

namespace Hydro
{
	// One per run, on the game session, so a new run starts fresh.
	struct BattleState
	{
		// Only while a game runs.
		static BattleState& Get();

		enum class Result : uint8_t { None, Won, Lost, Fled };

		static constexpr int MaxEnemies = 3; // the enemy slots in Battle.hyscene
		static constexpr int MaxMomentum = 5;

		bool Active = false;               // a battle was started and is not settled yet
		uint64_t EnemyId = 0;              // UUID of the field enemy that started it
		Result Outcome = Result::None;     // set by the battle, read by that enemy when its scene resumes
		// Rows of Enemies.hytable in slot order; "" is an empty slot.
		std::vector<std::string> Encounter{ "Slime" };
		// Rolled on top of each enemy's loot; unset: none.
		DataRowRef EncounterLoot;
		// Weaknesses found per enemy row and slot, kept for the whole run like a bestiary.
		std::unordered_map<std::string, std::array<bool, 3>> Revealed;

		bool IsRevealed(const std::string& row, int slot) const
		{
			auto found = Revealed.find(row);
			return found != Revealed.end() && slot >= 0 && slot < 3 && found->second[(size_t)slot];
		}

		// `starter` reads Outcome when its scene resumes. The caller pushes the Battle scene. False while another battle is
		// unsettled.
		bool BeginEncounter(uint64_t starter, const EncounterDefinition& encounter)
		{
			if (Active)
			{
				return false;
			}
			Active = true;
			EnemyId = starter;
			Outcome = Result::None;
			Encounter.clear();
			for (const DataRowRef& member : encounter.Enemies)
			{
				if ((int)Encounter.size() < MaxEnemies && member.IsSet())
				{
					Encounter.push_back(member.Row);
				}
			}
			EncounterLoot = encounter.Loot;
			return true;
		}

		// Result::None until the battle is over (or for another starter); taking it settles the hand-off.
		Result TakeOutcome(uint64_t starter)
		{
			if (Active == false || EnemyId != starter || Outcome == Result::None)
			{
				return Result::None;
			}
			const Result outcome = Outcome;
			Active = false;
			Outcome = Result::None;
			EnemyId = 0;
			return outcome;
		}
	};
}
