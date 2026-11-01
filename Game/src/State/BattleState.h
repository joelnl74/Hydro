// Game/src/State/BattleState.h
#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// The party and the battle hand-off: the run's battle state (RpgSession::Battle) and BattleData, the battle's
// numbers read from data (the Scripting plan's SC7). Moved out of the battle script (SC8a).

namespace Hydro
{
	// The party is Miko alone (HeroCount); she carries the party's HP and MP, so she is sturdier than Orin and Lila
	// were on their own. Her Attack walks up to the target and punches (a boost adds a kick, a punch, a kick); her two
	// skill commands are the first two abilities she knows (Flame Kick and Holy Palm; the ones levels teach are known
	// but have no command until the battle gets a skill list). The impacts land on the clips' contact frames
	// (BattleScript PunchImpactTime, KickImpactTime).
	constexpr int HeroCount = 1;

	// A hero's numbers at a level (HeroDefinition: Base + GrowthPerLevel for every level above 1): what the battle
	// and the field's menu read.
	struct HeroStats
	{
		std::string Name;
		int Level = 1;
		int MaxHP = 10;
		int MaxMP = 0;
		int Attack = 5;
		int Magic = 0;
		int Speed = 5;
		DamageType WeaponType = DamageType::Blunt; // the plain attack's type (the weapon's, unless the attack has one)
		std::string IdleClip = "BattleIdle";
		std::string ItemClip = "BattleIdle";
	};

	// The battle's numbers from data (the Scripting plan's SC7): the heroes' files, Assets/Data/Enemies.hytable,
	// Encounters.hytable, Abilities.hytable and Characters/LevelCurve.hydef. Looked up once a frame: the views keep
	// their files alive until the next frame, so the references handed out are good for the frame and an edit shows
	// from the next one. A missing file or row gives the type's defaults (and one warning, from Data).
	class BattleData
	{
	public:
		static constexpr uint64_t EnemyTable = 8285570869007471512ull;     // Assets/Data/Enemies.hytable
		static constexpr uint64_t EncounterTable = 5637391413083366177ull; // Assets/Data/Encounters.hytable
		static constexpr uint64_t LevelCurveFile = 1490334946040604597ull; // Assets/Data/Characters/LevelCurve.hydef
		// The party, in order: each hero's file, and the name its scene entities carry ("Hero Miko", "Status Miko").
		static constexpr uint64_t HeroFiles[HeroCount] = { 7766402021591977760ull }; // Assets/Data/Characters/Miko.hydef
		static constexpr const char* HeroIds[HeroCount] = { "Miko" };

		static const HeroDefinition& HeroData(int index)
		{
			const DataView<HeroDefinition>& view = Refresh().Heroes[std::clamp(index, 0, HeroCount - 1)];
			static const HeroDefinition none;
			return view.IsValid() ? *view : none;
		}

		// Hero `index` at `level`.
		static HeroStats StatsAt(int index, int level)
		{
			const HeroDefinition& hero = HeroData(index);
			const int steps = std::max(0, level - 1);
			HeroStats stats;
			stats.Name = hero.Name;
			stats.Level = std::max(1, level);
			stats.MaxHP = std::max(1, hero.Base.MaxHP + steps * hero.GrowthPerLevel.MaxHP);
			stats.MaxMP = std::max(0, hero.Base.MaxMP + steps * hero.GrowthPerLevel.MaxMP);
			stats.Attack = std::max(0, hero.Base.Attack + steps * hero.GrowthPerLevel.Attack);
			stats.Magic = std::max(0, hero.Base.Magic + steps * hero.GrowthPerLevel.Magic);
			stats.Speed = std::max(0, hero.Base.Speed + steps * hero.GrowthPerLevel.Speed);
			const DamageType attackType = Ability(hero.BasicAttack).Type;
			stats.WeaponType = attackType != DamageType::None ? attackType : hero.WeaponType;
			stats.IdleClip = hero.IdleClip;
			stats.ItemClip = hero.ItemClip;
			return stats;
		}

		// Hero `index` at the level the party has reached (BattleState); defined below it.
		static const HeroStats& Hero(int index);

		// An enemy row of Enemies.hytable ("" or a missing row: the defaults).
		static const EnemyDefinition& Enemy(const std::string& row)
		{
			static const EnemyDefinition none;
			if (row.empty())
			{
				return none;
			}
			Cache& cache = Refresh();
			auto found = cache.Enemies.find(row);
			if (found == cache.Enemies.end())
			{
				found = cache.Enemies.emplace(row, Data::GetRow<EnemyDefinition>(DataRowRef{ EnemyTable, row })).first;
			}
			return found->second.IsValid() ? *found->second : none;
		}

		// Any ability by its row (an unset one: the defaults).
		static const AbilityDefinition& Ability(const DataRowRef& reference)
		{
			static const AbilityDefinition none;
			if (reference.IsSet() == false)
			{
				return none;
			}
			Cache& cache = Refresh();
			const std::string key = std::to_string((uint64_t)reference.Table) + "/" + reference.Row;
			auto found = cache.Abilities.find(key);
			if (found == cache.Abilities.end())
			{
				found = cache.Abilities.emplace(key, Data::GetRow<AbilityDefinition>(reference)).first;
			}
			return found->second.IsValid() ? *found->second : none;
		}

		// Skill `slot` (0: the first skill command, 1: the second) of hero `index`: the abilities it knows, in order
		// (BattleState); defined below it.
		static const AbilityDefinition& Skill(int index, int slot);

		// The experience each level takes: [0] reaches level 2.
		static const std::vector<int>& ExperienceToLevel()
		{
			const DataView<LevelCurve>& view = Refresh().Curve;
			static const std::vector<int> none;
			return view.IsValid() ? view->ExperienceToLevel : none;
		}

		static int MaxLevel()
		{
			return (int)ExperienceToLevel().size() + 1;
		}

	private:
		struct Cache
		{
			uint64_t Frame = UINT64_MAX;
			DataView<HeroDefinition> Heroes[HeroCount];
			DataView<LevelCurve> Curve;
			std::unordered_map<std::string, DataView<EnemyDefinition>> Enemies;
			std::unordered_map<std::string, DataView<AbilityDefinition>> Abilities;
		};

		static Cache& Refresh()
		{
			static Cache cache;
			const uint64_t frame = Time::GetFrameCount();
			if (cache.Frame == frame)
			{
				return cache;
			}
			cache.Frame = frame;
			for (int h = 0; h < HeroCount; h++)
			{
				cache.Heroes[h] = Data::Get<HeroDefinition>(AssetHandle(HeroFiles[h]));
			}
			cache.Curve = Data::Get<LevelCurve>(AssetHandle(LevelCurveFile));
			cache.Enemies.clear();
			cache.Abilities.clear();
			return cache;
		}
	};

	inline const HeroStats& GetHero(int index)
	{
		return BattleData::Hero(index);
	}

	inline const EnemyDefinition& GetEnemy(const std::string& row)
	{
		return BattleData::Enemy(row);
	}

	// What the field and the battle scene tell each other, and what the party keeps between battles. One per run:
	// it lives on the game session (RpgSession::Battle, Scripting plan SC2), so a new run starts fresh.
	struct BattleState
	{
		// The running session's (RpgSession::Battle). Only while a game runs.
		static BattleState& Get();

		enum class Result : uint8_t { None, Won, Lost, Fled };

		static constexpr int MaxEnemies = 3;
		static constexpr int MaxMomentum = 5;

		// Started from the heroes' data the first time a field or battle scene needs them (StartParty), not when the
		// program starts: the data isn't loaded then.
		int HeroHP[HeroCount] = {};
		int HeroMP[HeroCount] = {};
		int HeroLevel[HeroCount] = {};
		int HeroExperience[HeroCount] = {};       // in total, since level 1
		std::vector<DataRowRef> HeroAbilities[HeroCount]; // known, in the order the menu uses
		bool PartyStarted = false;
		bool Active = false;               // a battle was started and has not been settled yet
		uint64_t EnemyId = 0;              // UUID of the field enemy that started it
		Result Outcome = Result::None;     // set by the battle for that enemy to read when it is back
		// The enemies (rows of Enemies.hytable; "" = an empty slot), in slot order.
		std::array<std::string, MaxEnemies> Encounter{ "Slime", "", "" };
		// The encounter's own loot table (EncounterDefinition::Loot), rolled on top of each enemy's; unset: none.
		DataRowRef EncounterLoot;
		// Weaknesses the party has found, per enemy row and weakness slot (kept for the whole run, as in a bestiary).
		std::unordered_map<std::string, std::array<bool, 3>> Revealed;

		void RestoreParty()
		{
			for (int i = 0; i < HeroCount; i++)
			{
				HeroHP[i] = GetHero(i).MaxHP;
				HeroMP[i] = GetHero(i).MaxMP;
			}
		}

		// Level 1 with the starting abilities and full HP and MP the first time; nothing after that.
		void StartParty()
		{
			if (PartyStarted)
			{
				return;
			}
			PartyStarted = true;
			for (int i = 0; i < HeroCount; i++)
			{
				HeroLevel[i] = 1;
				HeroExperience[i] = 0;
				HeroAbilities[i] = BattleData::HeroData(i).StartingAbilities;
			}
			RestoreParty();
		}

		bool IsRevealed(const std::string& row, int slot) const
		{
			auto found = Revealed.find(row);
			return found != Revealed.end() && slot >= 0 && slot < 3 && found->second[(size_t)slot];
		}

		// Experience for every hero still standing; a hero passing a level's mark (BattleData::ExperienceToLevel) goes
		// up a level: its maximums rise (and its HP and MP by as much), and the abilities that level teaches are known
		// from then on. Returns a message for each level up ("Miko reached level 3! Learned Radiant Kick.").
		std::vector<std::string> GainExperience(int amount)
		{
			std::vector<std::string> messages;
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			for (int h = 0; h < HeroCount; h++)
			{
				if (HeroHP[h] <= 0 || amount <= 0)
				{
					continue;
				}
				HeroExperience[h] += amount;
				while (HeroLevel[h] - 1 < (int)curve.size() && HeroExperience[h] >= curve[(size_t)(HeroLevel[h] - 1)])
				{
					const HeroStats before = BattleData::StatsAt(h, HeroLevel[h]);
					HeroLevel[h]++;
					const HeroStats after = BattleData::StatsAt(h, HeroLevel[h]);
					HeroHP[h] = std::min(after.MaxHP, HeroHP[h] + std::max(0, after.MaxHP - before.MaxHP));
					HeroMP[h] = std::min(after.MaxMP, HeroMP[h] + std::max(0, after.MaxMP - before.MaxMP));
					std::string message = after.Name + " reached level " + std::to_string(HeroLevel[h]) + "!";
					for (const LearnedAbility& learned : BattleData::HeroData(h).Learns)
					{
						const bool known = std::find(HeroAbilities[h].begin(), HeroAbilities[h].end(), learned.Ability) != HeroAbilities[h].end();
						if (learned.Level == HeroLevel[h] && learned.Ability.IsSet() && known == false)
						{
							HeroAbilities[h].push_back(learned.Ability);
							message += " Learned " + BattleData::Ability(learned.Ability).Name + ".";
						}
					}
					HY_INFO("Battle: {}", message);
					messages.push_back(message);
				}
			}
			return messages;
		}
	};

	inline const HeroStats& BattleData::Hero(int index)
	{
		static HeroStats stats[HeroCount];
		const int clamped = std::clamp(index, 0, HeroCount - 1);
		const int level = GameSession::IsRunning() ? std::max(1, BattleState::Get().HeroLevel[clamped]) : 1;
		stats[clamped] = StatsAt(clamped, level);
		return stats[clamped];
	}

	inline const AbilityDefinition& BattleData::Skill(int index, int slot)
	{
		static const AbilityDefinition none;
		if (GameSession::IsRunning() == false)
		{
			return none;
		}
		const std::vector<DataRowRef>& known = BattleState::Get().HeroAbilities[std::clamp(index, 0, HeroCount - 1)];
		return slot >= 0 && slot < (int)known.size() ? Ability(known[(size_t)slot]) : none;
	}
}
