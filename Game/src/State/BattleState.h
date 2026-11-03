#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Data/ItemDefinition.h"
#include "Rules/BattleRules.h"
#include "Rules/PartyRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// The party and the battle hand-off (BattleState, on the game session) and the battle's numbers read from data (BattleData).

namespace Hydro
{
	// Party order: Miko from the start, Orin Kenshi once he joins in the countryside. Every per-hero array is HeroCount
	// long; who is in the party now is BattleState::InParty.
	constexpr int HeroCount = 2;

	// A hero's numbers at a level: Base plus GrowthPerLevel for every level above 1.
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

	// Looked up once a frame: the views keep their files alive until the next frame, so references are good for the frame.
	// A missing file or row gives the type's defaults (and one warning, from Data).
	class BattleData
	{
	public:
		static constexpr uint64_t EnemyTable = 8285570869007471512ull;     // Assets/Data/Enemies.hytable
		static constexpr uint64_t EncounterTable = 5637391413083366177ull; // Assets/Data/Encounters.hytable
		static constexpr uint64_t LevelCurveFile = 1490334946040604597ull; // Assets/Data/Characters/LevelCurve.hydef
		// Each hero's file; HeroIds is the name its scene entities carry ("Hero Miko", "Status Miko").
		static constexpr uint64_t HeroFiles[HeroCount] = {
			7766402021591977760ull, // Assets/Data/Characters/Miko.hydef
			2716342829184015873ull, // Assets/Data/Characters/Orin.hydef
		};
		static constexpr const char* HeroIds[HeroCount] = { "Miko", "Orin" };

		static const HeroDefinition& HeroData(int index)
		{
			const DataView<HeroDefinition>& view = Refresh().Heroes[std::clamp(index, 0, HeroCount - 1)];
			static const HeroDefinition none;
			return view.IsValid() ? *view : none;
		}

		static HeroStats StatsAt(int index, int level)
		{
			const HeroDefinition& hero = HeroData(index);
			const CombatStats numbers = BattleRules::StatsAt(hero, level);
			HeroStats stats;
			stats.Name = hero.Name;
			stats.Level = std::max(1, level);
			stats.MaxHP = numbers.MaxHP;
			stats.MaxMP = numbers.MaxMP;
			stats.Attack = numbers.Attack;
			stats.Magic = numbers.Magic;
			stats.Speed = numbers.Speed;
			const DamageType attackType = Ability(hero.BasicAttack).Type;
			stats.WeaponType = attackType != DamageType::None ? attackType : hero.WeaponType;
			stats.IdleClip = hero.IdleClip;
			stats.ItemClip = hero.ItemClip;
			return stats;
		}

		// Each item's Bonus is added; the Equipment page previews a change with it.
		template<size_t N>
		static HeroStats StatsWith(int index, int level, const std::array<DataRowRef, N>& equipment)
		{
			HeroStats hero = StatsAt(index, level);
			for (const DataRowRef& worn : equipment)
			{
				if (worn.IsSet() == false)
				{
					continue;
				}
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(worn);
				if (item.IsValid() == false)
				{
					continue;
				}
				hero.MaxHP = std::max(1, hero.MaxHP + item->Bonus.MaxHP);
				hero.MaxMP = std::max(0, hero.MaxMP + item->Bonus.MaxMP);
				hero.Attack = std::max(0, hero.Attack + item->Bonus.Attack);
				hero.Magic = std::max(0, hero.Magic + item->Bonus.Magic);
				hero.Speed = std::max(0, hero.Speed + item->Bonus.Speed);
			}
			return hero;
		}

		// At the party's level, with what it wears; defined below BattleState.
		static const HeroStats& Hero(int index);

		// "" or a missing row: the defaults.
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

		// An unset row: the defaults.
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

		// An empty slot gives the defaults (an empty Name); defined below BattleState.
		static const AbilityDefinition& Skill(int index, int slot);

		// [0] reaches level 2.
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

	// What the field and the battle tell each other, and what the party keeps between battles. One per run, on the game
	// session, so a new run starts fresh.
	struct BattleState
	{
		// Only while a game runs.
		static BattleState& Get();

		enum class Result : uint8_t { None, Won, Lost, Fled };

		static constexpr int MaxEnemies = 3;
		static constexpr int MaxMomentum = 5;

		// Filled by StartParty the first time a scene needs them: the data isn't loaded when the program starts.
		int HeroHP[HeroCount] = {};
		int HeroMP[HeroCount] = {};
		int HeroLevel[HeroCount] = {};
		int HeroExperience[HeroCount] = {};       // in total, since level 1
		bool HeroJoined[HeroCount] = {};          // in the party now
		std::vector<DataRowRef> HeroAbilities[HeroCount]; // known, in the order learned
		// Set in the field menu's Abilities page; a newly learned ability takes the first empty one. Empty: an unset row.
		static constexpr int AbilitySlotCount = 8;
		std::array<DataRowRef, AbilitySlotCount> HeroAbilitySlots[HeroCount];
		// Empty: an unset row. Equipped items are not in the inventory (Rules/PartyRules.h moves them).
		static constexpr int EquipSlotCount = 6;
		std::array<DataRowRef, EquipSlotCount> HeroEquipment[HeroCount];
		bool PartyStarted = false;
		bool Active = false;               // a battle was started and is not settled yet
		uint64_t EnemyId = 0;              // UUID of the field enemy that started it
		Result Outcome = Result::None;     // set by the battle, read by that enemy when its scene resumes
		// Rows of Enemies.hytable in slot order; "" is an empty slot.
		std::array<std::string, MaxEnemies> Encounter{ "Slime", "", "" };
		// Rolled on top of each enemy's loot; unset: none.
		DataRowRef EncounterLoot;
		// Weaknesses found per enemy row and slot, kept for the whole run like a bestiary.
		std::unordered_map<std::string, std::array<bool, 3>> Revealed;

		void RestoreParty()
		{
			for (int i = 0; i < HeroCount; i++)
			{
				HeroHP[i] = GetHero(i).MaxHP;
				HeroMP[i] = GetHero(i).MaxMP;
			}
		}

		// Only the first time: level 1, the starting abilities, full HP and MP. Only Miko is in the party at the start.
		void StartParty()
		{
			if (PartyStarted)
			{
				return;
			}
			PartyStarted = true;
			for (int i = 0; i < HeroCount; i++)
			{
				SetUpHero(i, 1);
				HeroJoined[i] = i == 0;
			}
			RestoreParty();
		}

		bool InParty(int hero) const
		{
			return hero >= 0 && hero < HeroCount && HeroJoined[hero];
		}

		bool IsStanding(int hero) const
		{
			return InParty(hero) && HeroHP[hero] > 0;
		}

		int PartySize() const
		{
			int count = 0;
			for (int h = 0; h < HeroCount; h++)
			{
				count += HeroJoined[h] ? 1 : 0;
			}
			return count;
		}

		// Joins at the party's highest level, knowing what its levels so far teach, with full HP and MP. False when already in.
		bool Join(int hero)
		{
			StartParty();
			if (hero < 0 || hero >= HeroCount || HeroJoined[hero])
			{
				return false;
			}
			int level = 1;
			for (int h = 0; h < HeroCount; h++)
			{
				level = HeroJoined[h] ? std::max(level, HeroLevel[h]) : level;
			}
			SetUpHero(hero, level);
			HeroJoined[hero] = true;
			HeroHP[hero] = GetHero(hero).MaxHP;
			HeroMP[hero] = GetHero(hero).MaxMP;
			HY_INFO("Party: {} joins at level {}", GetHero(hero).Name, level);
			return true;
		}

		bool IsRevealed(const std::string& row, int slot) const
		{
			auto found = Revealed.find(row);
			return found != Revealed.end() && slot >= 0 && slot < 3 && found->second[(size_t)slot];
		}

		// For a guest who only fought one battle. Miko always stays.
		void Leave(int hero)
		{
			if (hero > 0 && hero < HeroCount)
			{
				HeroJoined[hero] = false;
			}
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
			Encounter = { std::string(), std::string(), std::string() };
			int count = 0;
			for (const DataRowRef& member : encounter.Enemies)
			{
				if (count < MaxEnemies && member.IsSet())
				{
					Encounter[(size_t)count++] = member.Row;
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

		// For every hero still standing; a level up raises the maximums (and HP and MP by as much) and teaches its abilities.
		// Returns a message per level up.
		std::vector<std::string> GainExperience(int amount)
		{
			std::vector<std::string> messages;
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			for (int h = 0; h < HeroCount; h++)
			{
				if (IsStanding(h) == false || amount <= 0)
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
						const bool known = std::find(HeroAbilities[h].begin(), HeroAbilities[h].end(), learned.Ability)
							!= HeroAbilities[h].end();
						if (learned.Level == HeroLevel[h] && learned.Ability.IsSet() && known == false)
						{
							HeroAbilities[h].push_back(learned.Ability);
							// Into the first empty battle slot, so it can be used at once.
							for (DataRowRef& slot : HeroAbilitySlots[h])
							{
								if (slot.IsSet() == false)
								{
									slot = learned.Ability;
									break;
								}
							}
							message += " Learned " + BattleData::Ability(learned.Ability).Name + ".";
						}
					}
					HY_INFO("Battle: {}", message);
					messages.push_back(message);
				}
			}
			return messages;
		}

	private:
		// The experience of that level's mark, the abilities up to it in the battle slots, nothing worn.
		void SetUpHero(int hero, int level)
		{
			const HeroDefinition& data = BattleData::HeroData(hero);
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			HeroLevel[hero] = std::max(1, level);
			HeroExperience[hero] = HeroLevel[hero] >= 2 && HeroLevel[hero] - 2 < (int)curve.size() ? curve[(size_t)(HeroLevel[hero] - 2)] : 0;
			HeroAbilities[hero] = data.StartingAbilities;
			for (const LearnedAbility& learned : data.Learns)
			{
				const bool known = std::find(HeroAbilities[hero].begin(), HeroAbilities[hero].end(), learned.Ability)
					!= HeroAbilities[hero].end();
				if (learned.Level <= HeroLevel[hero] && learned.Ability.IsSet() && known == false)
				{
					HeroAbilities[hero].push_back(learned.Ability);
				}
			}
			HeroAbilitySlots[hero] = {};
			for (size_t a = 0; a < HeroAbilities[hero].size() && a < (size_t)AbilitySlotCount; a++)
			{
				HeroAbilitySlots[hero][a] = HeroAbilities[hero][a];
			}
			HeroEquipment[hero] = {};
		}
	};

	inline const HeroStats& BattleData::Hero(int index)
	{
		static HeroStats stats[HeroCount];
		const int clamped = std::clamp(index, 0, HeroCount - 1);
		const int level = GameSession::IsRunning() ? std::max(1, BattleState::Get().HeroLevel[clamped]) : 1;
		stats[clamped] = GameSession::IsRunning() ? StatsWith(clamped, level, BattleState::Get().HeroEquipment[clamped])
			: StatsAt(clamped, level);
		return stats[clamped];
	}

	inline const AbilityDefinition& BattleData::Skill(int index, int slot)
	{
		static const AbilityDefinition none;
		if (GameSession::IsRunning() == false)
		{
			return none;
		}
		const auto& slots = BattleState::Get().HeroAbilitySlots[std::clamp(index, 0, HeroCount - 1)];
		return slot >= 0 && slot < (int)slots.size() ? Ability(slots[(size_t)slot]) : none;
	}
}
