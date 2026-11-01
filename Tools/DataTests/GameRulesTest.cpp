// Tools/DataTests/GameRulesTest.cpp
// The game's rules (Game/src/Rules: BattleRules, LootRules, Scripting plan SC8d) on fixed numbers: no scene, no data
// files, every random draw given. Built into DataRoundTripTest by scripts/test_data.sh.

#include "hypch.h"
#include "Rules/BattleRules.h"
#include "Rules/LootRules.h"

#include <cstdio>
#include <deque>

namespace Hydro
{
	static int s_RuleFailures = 0;

	static void RuleCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_RuleFailures++;
			std::printf("FAIL  GameRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define RULE_CHECK(condition) RuleCheck((condition), #condition, __LINE__, checks)

	// Hands out the given rolls in order, then 0.
	static std::function<float()> Rolls(std::deque<float> values)
	{
		auto queue = std::make_shared<std::deque<float>>(std::move(values));
		return [queue]() -> float
		{
			if (queue->empty())
			{
				return 0.0f;
			}
			const float value = queue->front();
			queue->pop_front();
			return value;
		};
	}

	int TestGameRules(int& checks)
	{
		std::printf("Game rules: battle and loot\n");

		EnemyDefinition slime;
		slime.Weak = { DamageType::Pierce, DamageType::Fire };
		slime.Resist = { DamageType::Blunt };

		// stat 10 x power 1, spread roll 0.5 (x1.0), no critical (roll 0.5).
		BattleRules::HitResult plain = BattleRules::ResolveHit(slime, DamageType::Slash, 10.0f, 1.0f, false, 0.5f, 0.5f);
		RULE_CHECK(plain.Damage == 10 && plain.Weak == false && plain.Resisted == false && plain.Critical == false && plain.WeakSlot == -1);
		BattleRules::HitResult weak = BattleRules::ResolveHit(slime, DamageType::Fire, 10.0f, 1.0f, false, 0.5f, 0.5f);
		RULE_CHECK(weak.Damage == 15 && weak.Weak && weak.WeakSlot == 1);
		BattleRules::HitResult resisted = BattleRules::ResolveHit(slime, DamageType::Blunt, 10.0f, 1.0f, false, 0.5f, 0.5f);
		RULE_CHECK(resisted.Damage == 5 && resisted.Resisted);
		BattleRules::HitResult critical = BattleRules::ResolveHit(slime, DamageType::Slash, 10.0f, 1.0f, false, 0.0f, 0.5f);
		RULE_CHECK(critical.Critical && critical.Damage == 15);
		BattleRules::HitResult broken = BattleRules::ResolveHit(slime, DamageType::Fire, 10.0f, 1.0f, true, 0.5f, 0.5f);
		RULE_CHECK(broken.Damage == 30);
		RULE_CHECK(BattleRules::ResolveHit(slime, DamageType::Blunt, 0.1f, 1.0f, false, 0.5f, 0.0f).Damage == 1); // at least 1

		RULE_CHECK(BattleRules::ResolveHeal(1.5f, 10, 1.0f, 0.5f) == 15);
		RULE_CHECK(BattleRules::EnemyAttackDamage(10, 1.0f, false, 0.375f) == 10);
		RULE_CHECK(BattleRules::EnemyAttackDamage(10, 1.0f, true, 0.375f) == 5);
		RULE_CHECK(BattleRules::EnemyAttackDamage(1, 0.1f, true, 0.0f) == 1);

		RULE_CHECK(BattleRules::IsBroken(3, 3) && BattleRules::IsBroken(3, 4) == false && BattleRules::IsBroken(-1, 0) == false);

		// Speeds 9, 4, (fallen), 7 with no luck: 0, 3, 1. Luck can swap close ones: 4 + 3 beats 6 + 0.
		RULE_CHECK((BattleRules::TurnOrder({ 9, 4, -1, 7 }, Rolls({ 0.0f, 0.0f, 0.0f })) == std::vector<int>{ 0, 3, 1 }));
		RULE_CHECK((BattleRules::TurnOrder({ 6, 4 }, Rolls({ 0.0f, 1.0f })) == std::vector<int>{ 1, 0 }));

		EnemyDefinition caster;
		caster.BasicAttack = DataRowRef{ 1, "Attack" };
		caster.Abilities = { EnemyAbility{ DataRowRef{ 1, "Spark" }, 3.0f } };
		RULE_CHECK(BattleRules::ChooseEnemyAction(caster, 0.1f).Row == "Attack"); // 0.4 of 4 is the attack's 1
		RULE_CHECK(BattleRules::ChooseEnemyAction(caster, 0.9f).Row == "Spark");
		RULE_CHECK(BattleRules::ChooseEnemyAction(slime, 0.9f).IsSet() == false); // nothing set: the (empty) plain attack

		const std::vector<int> curve = { 30, 75, 135 };
		RULE_CHECK(BattleRules::LevelFor(curve, 0) == 1 && BattleRules::LevelFor(curve, 29) == 1 && BattleRules::LevelFor(curve, 30) == 2);
		RULE_CHECK(BattleRules::LevelFor(curve, 75) == 3 && BattleRules::LevelFor(curve, 9999) == 4); // the cap: 3 marks + 1

		HeroDefinition miko;
		miko.Base = CombatStats{ 90, 32, 10, 8, 9 };
		miko.GrowthPerLevel = CombatStats{ 9, 3, 2, 2, 0 };
		const CombatStats level3 = BattleRules::StatsAt(miko, 3);
		RULE_CHECK(level3.MaxHP == 108 && level3.MaxMP == 38 && level3.Attack == 14 && level3.Magic == 12 && level3.Speed == 9);
		RULE_CHECK(BattleRules::StatsAt(miko, 1).MaxHP == 90 && BattleRules::StatsAt(miko, 0).MaxHP == 90);

		// Loot: a rarity by share, then an item of it, then a count.
		LootTableDefinition table;
		table.DropChance = 50.0f;
		table.Rolls = 3;
		table.Rarities = { RarityChance{ ItemRarity::Common, 75.0f }, RarityChance{ ItemRarity::Rare, 25.0f }, RarityChance{ ItemRarity::Epic, 50.0f } };
		table.Items = { LootEntry{ DataRowRef{ 7, "Potion" }, 1, 3 }, LootEntry{ DataRowRef{ 7, "Herb" }, 1, 1 }, LootEntry{ DataRowRef{ 7, "Feather" }, 1, 1 } };
		const LootRules::RarityLookup rarity = [](const DataRowRef& item) { return item.Row == "Feather" ? ItemRarity::Rare : ItemRarity::Common; };
		// Roll 1: 0.2 drops; rarity 0.5 of 100 (Epic has no items, so Common 75 + Rare 25) -> Common; item 0.0 -> Potion;
		// count 0.99 -> 3. Roll 2: 0.9 doesn't drop. Roll 3: 0.1 drops; rarity 0.9 -> Rare; item 0.0 -> Feather.
		const std::vector<LootRules::Drop> drops = LootRules::Roll(table, Rolls({ 0.2f, 0.5f, 0.0f, 0.99f, 0.9f, 0.1f, 0.9f, 0.0f, 0.0f }), rarity);
		RULE_CHECK(drops.size() == 2);
		RULE_CHECK(drops.size() == 2 && drops[0].Item.Row == "Potion" && drops[0].Count == 3 && drops[1].Item.Row == "Feather" && drops[1].Count == 1);
		// The same item twice adds up.
		table.Rolls = 2;
		table.DropChance = 100.0f;
		const std::vector<LootRules::Drop> twice = LootRules::Roll(table, Rolls({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }), rarity);
		RULE_CHECK(twice.size() == 1 && twice[0].Item.Row == "Potion" && twice[0].Count == 2);
		// No rarities: any item.
		table.Rarities.clear();
		table.Rolls = 1;
		RULE_CHECK(LootRules::Roll(table, Rolls({ 0.0f, 0.99f, 0.0f }), rarity)[0].Item.Row == "Feather");

		return s_RuleFailures;
	}
}
