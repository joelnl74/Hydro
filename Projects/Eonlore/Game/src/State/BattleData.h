#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Data/ItemDefinition.h"
#include "Rules/BattleRules.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// The battle's and the party's numbers read from data: heroes, the party, enemies, abilities and the level curve.

namespace Hydro
{
	// A hero's numbers at a level: Base plus GrowthPerLevel for every level above 1, plus what it wears.
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
		static constexpr uint64_t HeroTable = 8210482519817847920ull;      // Assets/Data/Characters/Heroes.hytable
		static constexpr uint64_t PartyFile = 192385608043738560ull;       // Assets/Data/Characters/Party.hydef
		static constexpr uint64_t EnemyTable = 8285570869007471512ull;     // Assets/Data/Enemies.hytable
		static constexpr uint64_t EncounterTable = 5637391413083366177ull; // Assets/Data/Encounters.hytable
		static constexpr uint64_t LevelCurveFile = 1490334946040604597ull; // Assets/Data/Characters/LevelCurve.hydef

		// A row of Heroes.hytable; an unknown ID gives the defaults.
		static const HeroDefinition& HeroData(const std::string& id)
		{
			static const HeroDefinition none;
			const HeroDefinition* hero = Refresh().Heroes.FindRow(id);
			return hero != nullptr ? *hero : none;
		}

		// Every hero's ID, sorted (the Battle scene played on its own fights with all of them).
		static std::vector<std::string> HeroIds()
		{
			std::vector<std::string> ids;
			for (const auto& [id, hero] : Refresh().Heroes.Rows())
			{
				(void)hero;
				ids.push_back(id);
			}
			return ids;
		}

		static const PartyDefinition& Party()
		{
			const DataView<PartyDefinition>& view = Refresh().Party;
			static const PartyDefinition none;
			return view.IsValid() ? *view : none;
		}

		static HeroStats StatsAt(const std::string& id, int level)
		{
			const HeroDefinition& hero = HeroData(id);
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
		static HeroStats StatsWith(const std::string& id, int level, const std::array<DataRowRef, N>& equipment)
		{
			HeroStats hero = StatsAt(id, level);
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
			DataTableView<HeroDefinition> Heroes;
			DataView<PartyDefinition> Party;
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
			cache.Heroes = Data::GetTable<HeroDefinition>(AssetHandle(HeroTable));
			cache.Party = Data::Get<PartyDefinition>(AssetHandle(PartyFile));
			cache.Curve = Data::Get<LevelCurve>(AssetHandle(LevelCurveFile));
			cache.Enemies.clear();
			cache.Abilities.clear();
			return cache;
		}
	};

	inline const EnemyDefinition& GetEnemy(const std::string& row)
	{
		return BattleData::Enemy(row);
	}
}
