// Game/src/Data/BattleDefinitions.h
#pragma once

#include "Hydro/Asset/Asset.h"
#include "Data/CombatStats.h"
#include "Data/LootDefinition.h"
#include "Hydro/Data/DataRowRef.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

// The battle's numbers (the Definitions plan's D5, #172; the Scripting plan's SC7): the hero (Assets/Data/Characters/
// <Hero>.hydef), the enemies (Enemies.hytable), who fights together (Encounters.hytable), every ability, the plain
// attack included (Abilities.hytable), and the level curve (Characters/LevelCurve.hydef). Only what the battle reads;
// the rules that use them are State/BattleState.h, Rules/ and the battle script.
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
		HY_PROPERTY(Tooltip("What it does, in a sentence (a skill list's help line)"))
		std::string Description;
		HY_PROPERTY(Tooltip("What the hit is made of (a heal ignores it); None on a hero's plain attack: the hero's weapon"))
		DamageType Type = DamageType::None;
		HY_PROPERTY(Range(0, 99), Tooltip("MP it costs"))
		int Cost = 0;
		HY_PROPERTY(Range(0, 10), Tooltip("Times the user's Attack (a strike) or Magic (a heal)"))
		float Power = 1.0f;
		HY_PROPERTY(Tooltip("Heals the most hurt ally instead of hitting a foe"))
		bool Heals = false;
		HY_PROPERTY(Tooltip("The hero's clip for it; \"Kick\" strikes as a kick, any other clip as a punch"))
		std::string Clip = "Punch";
		HY_PROPERTY(Tooltip("Walks up to the target first (a strike) instead of acting from where the hero stands"))
		bool Approach = true;
	};

	// An ability a hero learns on reaching a level.
	HY_DATA()
	struct LearnedAbility
	{
		HY_PROPERTY(Range(2, 99), Tooltip("The level that teaches it"))
		int Level = 2;
		HY_PROPERTY(DataRow(AbilityDefinition))
		DataRowRef Ability;
	};

	// A hero: one file each (Assets/Data/Characters/<Hero>.hydef).
	HY_DATA(Definition)
	struct HeroDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the battle's status and log, and in the field's menu"))
		std::string Name;
		HY_PROPERTY(Tooltip("Shown where there is little room; empty = Name"))
		std::string ShortName;
		HY_PROPERTY(Tooltip("The numbers at level 1"))
		CombatStats Base;
		HY_PROPERTY(Tooltip("Added for every level above 1"))
		CombatStats GrowthPerLevel{ 0, 0, 0, 0, 0 };
		HY_PROPERTY(Tooltip("What the plain attack hits with"))
		DamageType WeaponType = DamageType::Blunt;
		HY_PROPERTY(DataRow(AbilityDefinition), Tooltip("The Attack command (its name; its type when not None)"))
		DataRowRef BasicAttack;
		HY_PROPERTY(DataRow(AbilityDefinition), Tooltip("Known from level 1, in this order; they fill the first battle ability slots"))
		std::vector<DataRowRef> StartingAbilities;
		HY_PROPERTY(Tooltip("Abilities levels teach (known from then on; each goes into the first empty battle slot)"))
		std::vector<LearnedAbility> Learns;
		HY_PROPERTY(Tooltip("The fighting-stance loop between actions (and while guarding)"))
		std::string IdleClip = "BattleIdle";
		HY_PROPERTY(Tooltip("The clip played when using an item"))
		std::string ItemClip = "BattleIdle";
		HY_PROPERTY(Asset(Texture2D), Tooltip("A sprite sheet the field menu's Character page shows a frame of as the portrait"))
		AssetHandle Portrait = 0;
		HY_PROPERTY(Range(0, 255), Tooltip("Which sprite of the Portrait sheet"))
		int PortraitSprite = 0;
	};

	// An ability an enemy may use, and how often compared with its others.
	HY_DATA()
	struct EnemyAbility
	{
		HY_PROPERTY(DataRow(AbilityDefinition))
		DataRowRef Ability;
		HY_PROPERTY(Range(0, 100), Tooltip("Chance compared with its other abilities"))
		float Weight = 1.0f;
	};

	// An enemy: a row of Enemies.hytable. A new enemy is a row here and one in an encounter: the battle's enemy slots
	// play its animations, and the field's EnemyChaseScript names it (or its encounter).
	HY_DATA(Table, DisplayField(Name))
	struct EnemyDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the target list, turn order and log"))
		std::string Name;
		HY_PROPERTY(Tooltip("Shown where there is little room; empty = Name"))
		std::string ShortName;
		HY_PROPERTY(Asset(SpriteAnimation), Tooltip("Its battle animations (the slot's sprite plays them)"))
		AssetHandle Animations = 0;
		HY_PROPERTY(Tooltip("The clip it plays in battle"))
		std::string IdleClip = "Idle";
		HY_PROPERTY()
		CombatStats Stats;
		HY_PROPERTY(Range(0, 9), Tooltip("Hits on a weakness it takes to break it"))
		int Shield = 0;
		HY_PROPERTY(Tooltip("What it is weak to (at most three)"))
		std::vector<DamageType> Weak;
		HY_PROPERTY(Tooltip("What it resists"))
		std::vector<DamageType> Resist;
		HY_PROPERTY(DataRow(AbilityDefinition), Tooltip("Its plain attack (the damage is its Attack times the Power)"))
		DataRowRef BasicAttack;
		HY_PROPERTY(Tooltip("Other abilities, by weight (not used by the battle yet: enemies only attack)"))
		std::vector<EnemyAbility> Abilities;
		HY_PROPERTY(Range(0, 99999), Tooltip("Given to the party when it is defeated"))
		int Experience = 0;
		HY_PROPERTY(Range(0, 99999), Tooltip("Aster (the world's money) the party picks up when it is defeated"))
		int Money = 0;
		HY_PROPERTY(DataRow(LootTableDefinition), Tooltip("Rolled once the battle is won (Loot.hytable); unset: nothing"))
		DataRowRef Loot;
		// How it looks and moves (presentation, not rules).
		HY_PROPERTY(Range(0, 3), Tooltip("Metres it floats above the floor, bobbing (0: it stands); floating ones also sway in the field"))
		float FloatHeight = 0.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("Its field speeds times this (wandering, chasing, going home)"))
		float FieldSpeed = 1.0f;
		HY_PROPERTY(Color, Tooltip("Its glow's colour in battle"))
		glm::vec3 GlowColor{ 1.0f };
		HY_PROPERTY(Range(0, 10), Tooltip("Its glow in battle (0: none)"))
		float GlowIntensity = 0.0f;
		HY_PROPERTY(Color, Tooltip("The puff of smoke it leaves when defeated"))
		glm::vec3 SmokeColor{ 0.85f, 0.9f, 0.8f };
	};

	// Who fights together: a row of Encounters.hytable, up to three enemies in slot order.
	HY_DATA(Table)
	struct EncounterDefinition
	{
		HY_PROPERTY(DataRow(EnemyDefinition), Tooltip("Up to three, in slot order"))
		std::vector<DataRowRef> Enemies;
		HY_PROPERTY(DataRow(LootTableDefinition), Tooltip("Rolled once the battle is won, on top of each enemy's own loot"))
		DataRowRef Loot;
	};

	// The experience each level takes (Assets/Data/Characters/LevelCurve.hydef), shared by the heroes.
	HY_DATA(Definition)
	struct LevelCurve
	{
		HY_PROPERTY(Tooltip("Total experience to reach level 2, 3, ... in order; its length + 1 is the level cap"))
		std::vector<int> ExperienceToLevel;
	};
}
