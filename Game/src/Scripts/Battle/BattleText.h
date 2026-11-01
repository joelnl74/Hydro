// Game/src/Scripts/Battle/BattleText.h
#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"

#include <cstdint>

namespace Hydro
{
	// The battle's own wording (commands and messages; once rows of Assets/Sprites/UI/BattleText.png): UI text, in
	// code until there is a text table (with localisation). Names of heroes, enemies and skills are data (BattleData).
	enum class BattleLabel : uint32_t
	{
		Attack = 0, Guard, Potion, Run,
		Weak, Resist, Break, Victory, Defeated, Escaped, Boost, NoMP, NoPotions, Critical, Guarding, NoEscape,
		Abilities, Items
	};

	// The words a label stands for, as the runtime UI writes them (the command menu and the message box).
	inline const char* GetLabelText(BattleLabel label)
	{
		switch (label)
		{
		case BattleLabel::Attack: return "Attack";
		case BattleLabel::Guard: return "Guard";
		case BattleLabel::Potion: return "Potion";
		case BattleLabel::Run: return "Run";
		case BattleLabel::Weak: return "Weak!";
		case BattleLabel::Resist: return "Resist";
		case BattleLabel::Break: return "Break!";
		case BattleLabel::Victory: return "Victory!";
		case BattleLabel::Defeated: return "Defeated...";
		case BattleLabel::Escaped: return "Got away!";
		case BattleLabel::Boost: return "Boost";
		case BattleLabel::NoMP: return "Not enough MP";
		case BattleLabel::NoPotions: return "None left";
		case BattleLabel::Critical: return "Critical!";
		case BattleLabel::Guarding: return "Guarding";
		case BattleLabel::NoEscape: return "Couldn't get away!";
		case BattleLabel::Abilities: return "Abilities";
		case BattleLabel::Items: return "Items";
		default: return "";
		}
	}

	// Cells of Assets/Sprites/UI/BattleIcons.png: row Icon, then row Digit (0-9 from DigitBase).
	struct BattleIcon
	{
		static constexpr uint32_t Unknown = 5;
		static constexpr uint32_t Shield = 6;
		static constexpr uint32_t PipEmpty = 7;
		static constexpr uint32_t PipFull = 8;
		static constexpr uint32_t Cursor = 9;
		static constexpr uint32_t Broken = 10;
		static constexpr uint32_t DigitBase = 12;
		static uint32_t ForType(DamageType type) { return (uint32_t)type; }
	};

	// ---------------------------------------------------------------------------------------------------------
	// Battle

	// The party battle: put it on one entity of Battle.hyscene (the "Battle Director"). The party against
	// up to three enemies (BattleState::Get().Encounter). Rounds: everyone acts once, fastest first (the order is
	// shown at the top). On a hero's turn: 1 attack, 2 / 3 skills, 4 guard, 5 potion, 6 run, Left / Right pick
	// the target, Up / Down the boost.
	//
	// Rules (prototype values, tune freely):
	//   - Weak hits do 1.5x and take one point off the enemy's shield; resisted hits do 0.5x.
	//   - Shield at 0: the enemy is broken: it loses its turns this round and the next and takes 2x damage,
	//     then its shield comes back.
	//   - Momentum: each hero gains 1 per round (not in a round after boosting), up to 5. Boosting spends 1 to 3:
	//     an attack hits once more per point, a skill gets stronger.
	//   - Weaknesses show as "?" until hit with them, and stay known for the rest of the game.
	// It finds the scene's entities by tag (see scripts/shrine_glade/battle.py, which edits the scene).
}
