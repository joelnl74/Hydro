#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"

#include <cstdint>

namespace Hydro
{
	// The battle's own UI wording, in code until there is a text table. Hero, enemy and skill names are data (BattleData).
	enum class BattleLabel : uint32_t
	{
		Attack = 0, Guard, Potion, Run,
		Weak, Resist, Break, Victory, Defeated, Escaped, Boost, NoMP, NoPotions, Critical, Guarding, NoEscape,
		Abilities, Items
	};

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
}
