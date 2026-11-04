#pragma once

#include "Hydro.h"
#include "Scripts/Battle/BattleActors.h"
#include "Scripts/Battle/BattleEntities.h"
#include "Scripts/Battle/BattleFlow.h"
#include "Scripts/Battle/BattleStage.h"
#include "Scripts/Battle/BattleText.h"
#include "Scripts/Battle/Combatants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// What the battle draws over the stage: the message banner, the damage numbers, the turn marker, the target cursor and
// the fade in and out.

namespace Hydro
{
	class BattleOverlay
	{
	public:
		void Bind(Scene& scene, BattleStage& stage)
		{
			m_Scene = &scene;
			m_Stage = &stage;
			for (int p = 0; p < PopupCount; p++)
			{
				for (int d = 0; d < 3; d++)
				{
					m_Popups[(size_t)p].Digits[(size_t)d] = BattleEntities::Find(scene,
						"Popup " + std::to_string(p) + " Digit " + std::to_string(d));
				}
			}
			m_Cursor = BattleEntities::Find(scene, "Target Cursor");
			m_TurnMarker = BattleEntities::Find(scene, "Turn Marker");
			m_MessagePanel = BattleEntities::Find(scene, "Battle Message Panel");
			m_MessageText = BattleEntities::Find(scene, "Battle Message Text");
			m_Fade = BattleEntities::Find(scene, "Battle Overlay");
			UI::SetVisible(Entity{ m_MessagePanel, &scene }, false);
		}

		void ShowMessage(BattleLabel label, float seconds = 1.2f)
		{
			m_MessageLabel = label;
			m_MessageWords = GetLabelText(label);
			m_MessageTime = seconds;
		}

		void ShowMessage(const std::string& text, float seconds = 1.2f)
		{
			m_MessageLabel = BattleLabel::Attack; // no special tint
			m_MessageWords = text;
			m_MessageTime = seconds;
		}

		// A number rising from `position` (layout units).
		void SpawnPopup(const glm::vec2& position, int value, const glm::vec4& color)
		{
			Popup& popup = m_Popups[(size_t)m_NextPopup];
			m_NextPopup = (m_NextPopup + 1) % PopupCount;
			popup.Position = position;
			popup.Value = std::clamp(value, 0, 999);
			popup.Color = color;
			popup.Time = 0.0f;
		}

		void Tick(float deltaTime)
		{
			m_MessageTime = std::max(0.0f, m_MessageTime - deltaTime);
			for (Popup& popup : m_Popups)
			{
				popup.Time += deltaTime;
			}
		}

		// The marker bobs over the hero whose turn it is; the cursor over the enemy the target list points at.
		void Present(const BattleFlow& flow, const Combatants& actors, const BattleActors& stage, bool targeting, float time)
		{
			Scene& scene = *m_Scene;
			const BattleFlow::Phase phase = flow.GetPhase();
			const BattleAction& action = flow.GetAction();
			const bool choosing = phase == BattleFlow::Phase::Command;
			const bool acting = phase == BattleFlow::Phase::Acting;
			if (BattleEntities::Alive(scene, m_TurnMarker))
			{
				const bool hero = actors.IsValid(action.Actor) && actors.Get(action.Actor).IsHero();
				const bool show = (choosing || acting) && hero;
				if (hero)
				{
					const BattleActors::Actor& marked = stage.Of(actors.Get(action.Actor));
					m_Stage->SetPosition(m_TurnMarker, m_Stage->ScreenAbove(marked.Base + marked.Offset, BattleActors::HeroMarkerHeight)
						+ glm::vec2(0.0f, 0.15f * std::sin(time * 6.0f)));
				}
				BattleEntities::SetColor(scene, m_TurnMarker, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}
			if (BattleEntities::Alive(scene, m_Cursor))
			{
				const bool show = choosing && targeting && actors.IsAlive(action.Target);
				const glm::vec2 base = actors.IsValid(action.Target) ? stage.Of(actors.Get(action.Target)).Base : glm::vec2(0.0f);
				m_Stage->SetPosition(m_Cursor, m_Stage->ScreenAbove(base, BattleActors::EnemyCursorHeight) + glm::vec2(0.0f,
					0.15f * std::sin(time * 8.0f)));
				BattleEntities::SetColor(scene, m_Cursor, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}

			BattleEntities::SetVisible(EntityOf(m_MessagePanel), m_MessageTime > 0.0f || flow.IsOver());
			Entity messageText = EntityOf(m_MessageText);
			UI::SetText(messageText, m_MessageWords);
			if (messageText.IsValid() && messageText.HasComponent<UITextComponent>())
			{
				const glm::vec3 tint = m_MessageLabel == BattleLabel::Break ? glm::vec3(1.0f, 0.45f, 0.3f)
					: m_MessageLabel == BattleLabel::Weak ? glm::vec3(1.0f, 0.75f, 0.3f) : glm::vec3(0.95f, 0.9f, 0.78f);
				messageText.GetComponent<UITextComponent>().Color = glm::vec4(tint, 1.0f);
			}

			for (Popup& popup : m_Popups)
			{
				const float t = popup.Time / 0.9f;
				const float alpha = t < 1.0f ? std::min(1.0f, 3.0f * (1.0f - t)) : 0.0f;
				SetNumber(popup.Digits, popup.Value, popup.Position + glm::vec2(0.0f, 0.9f * std::min(t, 1.0f)), 0.55f,
					glm::vec4(popup.Color.r, popup.Color.g, popup.Color.b, alpha));
			}

			const float phaseTime = flow.GetPhaseTime();
			float fade = 0.0f;
			if (phase == BattleFlow::Phase::Intro)
			{
				fade = 1.0f - std::min(1.0f, phaseTime / 0.6f);
			}
			else if (phase == BattleFlow::Phase::Victory || phase == BattleFlow::Phase::Defeat)
			{
				fade = std::clamp((phaseTime - 1.2f) / 0.7f, 0.0f, 1.0f);
			}
			else if (phase == BattleFlow::Phase::Fleeing)
			{
				fade = std::clamp((phaseTime - 0.3f) / 0.5f, 0.0f, 1.0f);
			}
			else if (phase == BattleFlow::Phase::Done)
			{
				fade = 1.0f;
			}
			BattleEntities::SetColor(scene, m_Fade, phase == BattleFlow::Phase::Defeat ? glm::vec4(0.35f, 0.0f, 0.0f, fade)
				: glm::vec4(0.0f, 0.0f, 0.0f, fade));
		}

	private:
		static constexpr int PopupCount = 3;

		struct Popup
		{
			std::array<entt::entity, 3> Digits = { entt::null, entt::null, entt::null };
			glm::vec2 Position{ 0.0f };
			int Value = 0;
			glm::vec4 Color{ 1.0f };
			float Time = 10.0f;
		};

		// Up to three digit sprites centred on `centre`; unused digits are hidden.
		void SetNumber(const std::array<entt::entity, 3>& digits, int value, const glm::vec2& centre, float spacing, const glm::vec4& color)
		{
			Scene& scene = *m_Scene;
			const std::string text = std::to_string(std::clamp(value, 0, 999));
			const int count = (int)text.size();
			for (int d = 0; d < 3; d++)
			{
				if (d < count)
				{
					BattleEntities::SetSprite(scene, digits[(size_t)d], BattleIcon::DigitBase + (uint32_t)(text[(size_t)d] - '0'));
					m_Stage->SetPosition(digits[(size_t)d], centre + glm::vec2(((float)d - 0.5f * (float)(count - 1)) * spacing, 0.0f));
					BattleEntities::SetColor(scene, digits[(size_t)d], color);
				}
				else
				{
					BattleEntities::SetColor(scene, digits[(size_t)d], glm::vec4(0.0f));
				}
			}
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		BattleStage* m_Stage = nullptr;
		std::array<Popup, PopupCount> m_Popups;
		int m_NextPopup = 0;
		entt::entity m_Cursor = entt::null;
		entt::entity m_TurnMarker = entt::null;
		entt::entity m_MessagePanel = entt::null;
		entt::entity m_MessageText = entt::null;
		entt::entity m_Fade = entt::null;
		BattleLabel m_MessageLabel = BattleLabel::Attack;
		std::string m_MessageWords;
		float m_MessageTime = 0.0f;
	};
}
