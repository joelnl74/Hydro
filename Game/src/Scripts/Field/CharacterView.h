#pragma once

#include "Hydro.h"
#include "Data/ItemDefinition.h"
#include "Rules/PartyRules.h"
#include "State/BattleState.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>

namespace Hydro
{
	// The field menu's Character page for one hero: portrait, level and experience, stats with what the equipment adds, and
	// what it wears. Nothing on it takes the focus.
	class CharacterView
	{
	public:
		static constexpr int StatCount = 5; // Max HP, Max MP, Attack, Magic, Speed

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Character page";
			m_Name = ViewHelpers::Find(scene, "Character Name", owner);
			m_Portrait = ViewHelpers::Find(scene, "Character Portrait", owner);
			m_Level = ViewHelpers::Find(scene, "Character Level", owner);
			m_Experience = ViewHelpers::Find(scene, "Character Experience", owner);
			m_Next = ViewHelpers::Find(scene, "Character Next", owner);
			m_ExperienceFill = ViewHelpers::Find(scene, "Character Experience Fill", owner);
			for (int i = 0; i < StatCount; i++)
			{
				m_Stats[i] = ViewHelpers::Find(scene, "Character Stat " + std::to_string(i) + " Value", owner);
			}
			for (int i = 0; i < BattleState::EquipSlotCount; i++)
			{
				m_SlotNames[i] = ViewHelpers::Find(scene, "Character Equip " + std::to_string(i) + " Name", owner);
				m_Equipped[i] = ViewHelpers::Find(scene, "Character Equip " + std::to_string(i) + " Value", owner);
			}
		}

		void Present(int h)
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const HeroDefinition& data = BattleData::HeroData(h);
			Entity portrait = EntityOf(m_Portrait);
			UIImageComponent* image = portrait.IsValid() ? portrait.TryGetComponent<UIImageComponent>() : nullptr;
			if (image != nullptr && (uint64_t)data.Portrait != 0)
			{
				AssetManager::GetAsset<Texture2D>(data.Portrait); // the renderer never loads
				image->Texture = data.Portrait;
				image->SpriteIndex = (uint32_t)std::max(0, data.PortraitSprite);
			}
			const BattleState& party = BattleState::Get();
			const int level = std::max(1, party.HeroLevel[h]);
			const HeroStats& worn = GetHero(h);                     // with equipment
			const HeroStats bare = BattleData::StatsAt(h, level);   // without
			UI::SetText(EntityOf(m_Name), worn.Name);
			UI::SetText(EntityOf(m_Level), "Level " + std::to_string(level));
			UI::SetText(EntityOf(m_Experience), "Experience " + std::to_string(party.HeroExperience[h]));

			// The marks are totals since level 1: [level - 2] reached this level, [level - 1] reaches the next.
			const std::vector<int>& curve = BattleData::ExperienceToLevel();
			if (level - 1 < (int)curve.size())
			{
				const int from = level >= 2 ? curve[(size_t)(level - 2)] : 0;
				const int to = curve[(size_t)(level - 1)];
				UI::SetText(EntityOf(m_Next), "Next level: " + std::to_string(std::max(0, to - party.HeroExperience[h])));
				ViewHelpers::SetBarFill(EntityOf(m_ExperienceFill),
					(float)(party.HeroExperience[h] - from) / (float)std::max(1, to - from));
			}
			else
			{
				UI::SetText(EntityOf(m_Next), "Highest level");
				ViewHelpers::SetBarFill(EntityOf(m_ExperienceFill), 1.0f);
			}

			const int withGear[StatCount] = { worn.MaxHP, worn.MaxMP, worn.Attack, worn.Magic, worn.Speed };
			const int without[StatCount] = { bare.MaxHP, bare.MaxMP, bare.Attack, bare.Magic, bare.Speed };
			for (int i = 0; i < StatCount; i++)
			{
				std::string value = std::to_string(withGear[i]);
				const int bonus = withGear[i] - without[i];
				if (bonus != 0)
				{
					value = (bonus > 0 ? "(+" : "(") + std::to_string(bonus) + ")  " + value;
				}
				UI::SetText(EntityOf(m_Stats[i]), value);
			}

			for (int i = 0; i < BattleState::EquipSlotCount; i++)
			{
				UI::SetText(EntityOf(m_SlotNames[i]), PartyRules::GetPositionName((EquipPosition)i));
				const DataRowRef& item = party.HeroEquipment[h][(size_t)i];
				const DataView<ItemDefinition> definition = item.IsSet() ? Data::GetRow<ItemDefinition>(item) : DataView<ItemDefinition>();
				UI::SetText(EntityOf(m_Equipped[i]),
					definition.IsValid() ? definition->Name : (item.IsSet() ? "Missing: " + item.Row : std::string("-")));
			}
		}

	private:
		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		entt::entity m_Portrait = entt::null;
		entt::entity m_Name = entt::null, m_Level = entt::null, m_Experience = entt::null, m_Next = entt::null,
			m_ExperienceFill = entt::null;
		entt::entity m_Stats[StatCount] = {};
		entt::entity m_SlotNames[BattleState::EquipSlotCount] = {};
		entt::entity m_Equipped[BattleState::EquipSlotCount] = {};
	};
}
