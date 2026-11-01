// Game/src/Data/BattleDefinitions.h
#pragma once

#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>
#include <vector>

// The battle's numbers (the Definitions plan's D5, #172): who fights (Assets/Data/Combatants.hytable: the heroes and
// the enemy kinds) and the skills they use (Assets/Data/Abilities.hytable). The rules that use them stay in
// BattleScripts.h.
namespace Hydro
{
	// What a hit is made of; enemies are weak to or resist some. The order is the battle's icons' (BattleIcon).
	HY_ENUM()
	enum class DamageType : uint8_t
	{
		Slash = 0,
		Pierce,
		Blunt,
		Fire,
		Light,
		None
	};

	HY_DATA(Table, DisplayField(Name))
	struct AbilityDefinition
	{
		HY_PROPERTY(Tooltip("The command's name and the message when it is used"))
		std::string Name;
		HY_PROPERTY(Tooltip("What the hit is made of (a heal ignores it)"))
		DamageType Type = DamageType::None;
		HY_PROPERTY(Range(0, 99), Tooltip("MP it costs"))
		int Cost = 0;
		HY_PROPERTY(Range(0, 10), Tooltip("Times the hero's Attack (a strike) or Magic (a heal)"))
		float Power = 1.0f;
		HY_PROPERTY(Tooltip("Heals the most hurt ally instead of hitting a foe"))
		bool Heals = false;
		HY_PROPERTY(Tooltip("The hero's clip for it; \"Kick\" strikes as a kick, any other clip as a punch"))
		std::string Clip = "Punch";
		HY_PROPERTY(Tooltip("Walks up to the target first (a strike) instead of acting from where the hero stands"))
		bool Approach = true;
	};

	// A hero or an enemy kind. Enemies use HP, Attack, Speed, Shield, Weak and Resist; heroes the rest too.
	HY_DATA(Definition, Table, DisplayField(Name))
	struct CombatantDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the battle's status, turn order and log"))
		std::string Name;
		HY_PROPERTY(Tooltip("Shown in the target list; empty = Name"))
		std::string ShortName;
		HY_PROPERTY(Range(1, 9999))
		int MaxHP = 10;
		HY_PROPERTY(Range(0, 999))
		int MaxMP = 0;
		HY_PROPERTY(Range(0, 999))
		int Attack = 5;
		HY_PROPERTY(Range(0, 999))
		int Magic = 0;
		HY_PROPERTY(Range(0, 99), Tooltip("Who acts first: higher goes earlier"))
		int Speed = 5;
		HY_PROPERTY(Tooltip("Heroes: what the plain Attack hits with"))
		DamageType WeaponType = DamageType::Blunt;
		HY_PROPERTY(DataRow(AbilityDefinition), Tooltip("Heroes: the two skill commands, in order"))
		std::vector<DataRowRef> Skills;
		HY_PROPERTY(Tooltip("Heroes: the clip played when using an item"))
		std::string ItemClip = "BattleIdle";
		HY_PROPERTY(Tooltip("Heroes: the fighting-stance loop between actions (and while guarding)"))
		std::string IdleClip = "BattleIdle";
		HY_PROPERTY(Range(0, 9), Tooltip("Enemies: hits on a weakness it takes to break it"))
		int Shield = 0;
		HY_PROPERTY(Tooltip("Enemies: what it is weak to (at most three)"))
		std::vector<DamageType> Weak;
		HY_PROPERTY(Tooltip("Enemies: what it resists"))
		std::vector<DamageType> Resist;
	};
}
