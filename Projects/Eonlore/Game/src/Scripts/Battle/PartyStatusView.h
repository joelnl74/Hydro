#pragma once

#include "Hydro.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/Combatants.h"
#include "State/BattleState.h"
#include "State/PartyState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// The party's status, top right: one block per party slot ("Status <n>") with the fighter's name, HP, MP and boost
// points. Blocks without a fighter are hidden, so the column closes up.

namespace Hydro
{
	class PartyStatusView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			for (int s = 0; s < PartyState::MaxFighters; s++)
			{
				const std::string status = "Status " + std::to_string(s);
				Block& block = m_Blocks[(size_t)s];
				block.Root = BattleEntities::Find(scene, status);
				block.Name = BattleEntities::Find(scene, status + " Name");
				block.HP = BattleEntities::Find(scene, status + " HP");
				block.MP = BattleEntities::Find(scene, status + " MP");
				block.HPFill = BattleEntities::Find(scene, status + " HP Fill");
				block.MPFill = BattleEntities::Find(scene, status + " MP Fill");
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					block.Pips[(size_t)p] = BattleEntities::Find(scene, status + " Pip " + std::to_string(p));
				}
			}
		}

		// Bars are sage, amber below half, red below a quarter. Full boost points are gold; the ones the boost is about to
		// spend pulse.
		void Present(const Combatants& actors, const BattleFlow& flow, float time)
		{
			std::array<bool, PartyState::MaxFighters> used = {};
			const BattleAction& action = flow.GetAction();
			const bool commanding = flow.GetPhase() == BattleFlow::Phase::Command;
			for (const Combatant& hero : actors.GetAll())
			{
				if (hero.IsHero() == false || hero.Slot < 0 || hero.Slot >= PartyState::MaxFighters)
				{
					continue;
				}
				used[(size_t)hero.Slot] = true;
				const Block& block = m_Blocks[(size_t)hero.Slot];
				const HeroStats stats = actors.GetHeroStats(hero.Id);
				const int hp = actors.GetHP(hero.Id);
				const int mp = actors.GetMP(hero.Id);
				const float hpFraction = (float)hp / (float)std::max(1, stats.MaxHP);
				const glm::vec4 hpColor = hpFraction > 0.5f ? glm::vec4(0.5f, 0.72f, 0.4f, 1.0f)
					: hpFraction > 0.25f ? glm::vec4(0.92f, 0.72f, 0.3f, 1.0f)
					: glm::vec4(0.86f, 0.3f, 0.24f, 1.0f);
				UI::SetText(EntityOf(block.Name), stats.Name);
				UI::SetText(EntityOf(block.HP), std::to_string(hp) + " / " + std::to_string(stats.MaxHP));
				UI::SetText(EntityOf(block.MP), std::to_string(mp) + " / " + std::to_string(stats.MaxMP));
				BattleEntities::SetFill(EntityOf(block.HPFill), hpFraction, hpColor);
				BattleEntities::SetFill(EntityOf(block.MPFill), (float)mp / (float)std::max(1, stats.MaxMP), glm::vec4(0.38f, 0.6f, 0.9f,
					1.0f));
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					const bool spending = commanding && action.Actor == hero.Id && p >= hero.Momentum - action.Boost && p < hero.Momentum;
					const float pulse = spending ? 0.55f + 0.45f * std::sin(time * 10.0f) : 1.0f;
					BattleEntities::SetUIColor(EntityOf(block.Pips[(size_t)p]),
						p < hero.Momentum ? glm::vec4(1.0f, 0.8f * pulse + 0.2f, 0.42f * pulse, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.22f));
				}
			}
			for (int s = 0; s < PartyState::MaxFighters; s++)
			{
				BattleEntities::SetVisible(EntityOf(m_Blocks[(size_t)s].Root), used[(size_t)s]);
			}
		}

	private:
		struct Block
		{
			entt::entity Root = entt::null, Name = entt::null, HP = entt::null, MP = entt::null, HPFill = entt::null, MPFill = entt::null;
			std::array<entt::entity, BattleState::MaxMomentum> Pips = {};
		};

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<Block, PartyState::MaxFighters> m_Blocks;
	};
}
