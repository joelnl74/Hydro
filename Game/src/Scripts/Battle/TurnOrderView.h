#pragma once

#include "Hydro.h"
#include "Scripts/Battle/BattleActors.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/Combatants.h"

#include <array>
#include <string>

// The turn order, top left: a chip per turn left in the round ("Order Chip <n>"), the actor's sprite inside.

namespace Hydro
{
	class TurnOrderView
	{
	public:
		static constexpr int ChipCount = 8; // scripts/game/ui/build_game_ui.py ORDER_CHIPS

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			for (int c = 0; c < ChipCount; c++)
			{
				m_Chips[(size_t)c] = BattleEntities::Find(scene, "Order Chip " + std::to_string(c));
				m_Actors[(size_t)c] = BattleEntities::Find(scene, "Order Chip " + std::to_string(c) + " Actor");
			}
		}

		// From the actor acting now (the big first chip) to the end of the round; enemies' chips are reddish.
		void Present(const BattleFlow& flow, const Combatants& actors, const BattleActors& stage)
		{
			Scene& scene = *m_Scene;
			const std::vector<int>& order = flow.GetOrder();
			int shown = 0;
			for (int index = std::max(flow.GetTurn(), 0); index < (int)order.size() && shown < ChipCount; index++)
			{
				const int who = order[(size_t)index];
				if (actors.IsAlive(who) == false)
				{
					continue;
				}
				const Combatant& combatant = actors.Get(who);
				Entity chip = EntityOf(m_Chips[(size_t)shown]);
				BattleEntities::SetVisible(chip, true);
				const bool now = index == flow.GetTurn();
				BattleEntities::SetUIColor(chip, combatant.IsHero() ? (now ? glm::vec4(1.0f, 0.9f, 0.62f, 1.0f) : glm::vec4(1.0f))
					: glm::vec4(1.0f, 0.72f, 0.66f, 1.0f));
				const entt::entity sprite = stage.Of(combatant).Sprite;
				Entity portrait = EntityOf(m_Actors[(size_t)shown]);
				if (portrait.IsValid() && portrait.HasComponent<UIImageComponent>() && BattleEntities::Alive(scene, sprite)
					&& scene.m_Registry.has<SpriteRendererComponent>(sprite))
				{
					const SpriteRendererComponent& renderer = scene.m_Registry.get<SpriteRendererComponent>(sprite);
					UIImageComponent& image = portrait.GetComponent<UIImageComponent>();
					image.Texture = renderer.Texture;
					image.SpriteIndex = renderer.SpriteIndex;
					image.Color = glm::vec4(1.0f);
				}
				shown++;
			}
			for (int c = shown; c < ChipCount; c++)
			{
				BattleEntities::SetVisible(EntityOf(m_Chips[(size_t)c]), false);
			}
		}

	private:
		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<entt::entity, ChipCount> m_Chips = {};
		std::array<entt::entity, ChipCount> m_Actors = {};
	};
}
