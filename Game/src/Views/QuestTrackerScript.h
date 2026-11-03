#pragma once

#include "Hydro.h"
#include "State/QuestLog.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// Each image's and text's own alpha is kept and multiplied by the group's.
	class QuestUIFade
	{
	public:
		void Collect(Scene& scene, Entity root)
		{
			m_Items.clear();
			if (root.IsValid() == false)
			{
				return;
			}
			std::vector<Entity> subtree;
			scene.GetSubtree(root, subtree);
			for (const Entity& entity : subtree)
			{
				Item item;
				item.Handle = entity;
				if (entity.HasComponent<UIImageComponent>())
				{
					item.Image = entity.GetComponent<UIImageComponent>().Color.a;
				}
				if (entity.HasComponent<UITextComponent>())
				{
					item.Text = entity.GetComponent<UITextComponent>().Color.a;
				}
				if (item.Image >= 0.0f || item.Text >= 0.0f)
				{
					m_Items.push_back(item);
				}
			}
		}

		void Set(float alpha)
		{
			for (Item& item : m_Items)
			{
				if (item.Handle.IsValid() == false)
				{
					continue;
				}
				if (item.Image >= 0.0f && item.Handle.HasComponent<UIImageComponent>())
				{
					item.Handle.GetComponent<UIImageComponent>().Color.a = item.Image * alpha;
				}
				if (item.Text >= 0.0f && item.Handle.HasComponent<UITextComponent>())
				{
					item.Handle.GetComponent<UITextComponent>().Color.a = item.Text * alpha;
				}
			}
		}

	private:
		struct Item
		{
			Entity Handle;
			float Image = -1.0f; // its own alpha; below 0 = none
			float Text = -1.0f;
		};
		std::vector<Item> m_Items;
	};

	// On "Quest Canvas": the tracked quest (top right) and the "New Quest" / "Quest Complete" banner, both shown only while
	// no menu, dialogue or cutscene holds the screen.
	HY_SCRIPT()
	class QuestTrackerScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0.5, 20), Tooltip("Seconds a banner stays fully shown"))
		float BannerHoldTime = 2.4f;
		HY_PROPERTY(Range(0.05, 5), Tooltip("Seconds a banner takes to open (and a little longer to fade out)"))
		float BannerOpenTime = 0.5f;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Tracker = Child(scene, "Quest Tracker");
			m_TrackerTitle = Child(scene, "Quest Tracker Title");
			m_TrackerObjective = Child(scene, "Quest Tracker Objective");
			m_TrackerCount = Child(scene, "Quest Tracker Count");
			m_Banner = Child(scene, "Quest Banner");
			m_BannerHeader = Child(scene, "Quest Banner Header");
			m_BannerTitle = Child(scene, "Quest Banner Title");
			m_BannerBand = Child(scene, "Quest Banner Band");
			m_BannerRules[0] = Child(scene, "Quest Banner Rule Top");
			m_BannerRules[1] = Child(scene, "Quest Banner Rule Bottom");
			for (int i = 0; i < 2; i++)
			{
				if (m_BannerRules[i].IsValid() && m_BannerRules[i].HasComponent<UIRectComponent>())
				{
					m_RuleWidth[i] = m_BannerRules[i].GetComponent<UIRectComponent>().SizeDelta.x;
				}
			}
			if (m_BannerBand.IsValid() && m_BannerBand.HasComponent<UIRectComponent>())
			{
				m_BandHeight = m_BannerBand.GetComponent<UIRectComponent>().SizeDelta.y;
			}
			if (m_TrackerTitle.IsValid() && m_TrackerTitle.HasComponent<UITextComponent>())
			{
				m_TitleColor = m_TrackerTitle.GetComponent<UITextComponent>().Color;
			}
			m_TrackerFade.Collect(scene, m_Tracker);
			m_BannerFade.Collect(scene, m_Banner);
			m_TrackerAlpha = 0.0f;
			m_TrackerFade.Set(0.0f);
			UI::SetVisible(m_Tracker, false);
			UI::SetVisible(m_Banner, false);
			m_Revision = QuestLog::Get().Revision;
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			const bool held = UI::CapturesInput(scene);
			UpdateBanner(held, deltaTime);
			UpdateTracker(held, deltaTime);
		}

	private:
		void UpdateTracker(bool held, float deltaTime)
		{
			const QuestLog::Quest* quest = QuestLog::Get().GetTracked();
			if (m_Revision != QuestLog::Get().Revision)
			{
				m_Revision = QuestLog::Get().Revision;
				m_Flash = 1.0f;
			}
			m_Flash = std::max(0.0f, m_Flash - deltaTime * 0.8f);

			const float target = (quest != nullptr && held == false && m_BannerTime < 0.0f) ? 1.0f : 0.0f;
			const float step = deltaTime * 3.0f;
			m_TrackerAlpha = m_TrackerAlpha < target ? std::min(target, m_TrackerAlpha + step) : std::max(target, m_TrackerAlpha - step);
			if (UI::IsVisible(m_Tracker) != (m_TrackerAlpha > 0.0f))
			{
				UI::SetVisible(m_Tracker, m_TrackerAlpha > 0.0f);
			}
			if (m_TrackerAlpha <= 0.0f || quest == nullptr)
			{
				return;
			}

			UI::SetText(m_TrackerTitle, quest->GetTitle());
			UI::SetText(m_TrackerObjective, quest->GetObjective());
			UI::SetText(m_TrackerCount,
				quest->Goal > 0 ? std::to_string(quest->Progress) + " / " + std::to_string(quest->Goal) : std::string());
			// Eased in from the right as it appears.
			if (m_Tracker.HasComponent<UIRectComponent>())
			{
				if (m_TrackerX == 0.0f && m_TrackerSlide == false)
				{
					m_TrackerX = m_Tracker.GetComponent<UIRectComponent>().AnchoredPosition.x;
					m_TrackerSlide = true;
				}
				const float eased = m_TrackerAlpha * m_TrackerAlpha * (3.0f - 2.0f * m_TrackerAlpha);
				m_Tracker.GetComponent<UIRectComponent>().AnchoredPosition.x = m_TrackerX + (1.0f - eased) * 60.0f;
			}
			m_TrackerFade.Set(m_TrackerAlpha);
			if (m_TrackerTitle.IsValid() && m_TrackerTitle.HasComponent<UITextComponent>())
			{
				const glm::vec4 gold{ 1.0f, 0.84f, 0.5f, m_TitleColor.a };
				glm::vec4 color = m_TitleColor + (gold - m_TitleColor) * m_Flash;
				color.a = m_TitleColor.a * m_TrackerAlpha;
				m_TrackerTitle.GetComponent<UITextComponent>().Color = color;
			}
		}

		void UpdateBanner(bool held, float deltaTime)
		{
			if (m_BannerTime < 0.0f)
			{
				if (held || QuestLog::Get().Announcements.empty())
				{
					return;
				}
				const QuestLog::Announcement next = QuestLog::Get().Announcements.front();
				QuestLog::Get().Announcements.erase(QuestLog::Get().Announcements.begin());
				UI::SetText(m_BannerHeader, next.Header);
				UI::SetText(m_BannerTitle, next.Title);
				UI::SetVisible(m_Banner, true);
				m_BannerTime = 0.0f;
			}

			m_BannerTime += deltaTime;
			const float close = BannerOpenTime * 1.4f;
			const float total = BannerOpenTime + BannerHoldTime + close;
			float alpha = 1.0f;
			float open = 1.0f;
			if (m_BannerTime < BannerOpenTime)
			{
				open = m_BannerTime / BannerOpenTime;
				alpha = open;
			}
			else if (m_BannerTime > BannerOpenTime + BannerHoldTime)
			{
				alpha = std::max(0.0f, 1.0f - (m_BannerTime - BannerOpenTime - BannerHoldTime) / close);
			}
			open = open * open * (3.0f - 2.0f * open);
			m_BannerFade.Set(alpha);
			for (int i = 0; i < 2; i++)
			{
				if (m_BannerRules[i].IsValid() && m_BannerRules[i].HasComponent<UIRectComponent>())
				{
					m_BannerRules[i].GetComponent<UIRectComponent>().SizeDelta.x = m_RuleWidth[i] * (0.2f + 0.8f * open);
				}
			}
			if (m_BannerBand.IsValid() && m_BannerBand.HasComponent<UIRectComponent>())
			{
				m_BannerBand.GetComponent<UIRectComponent>().SizeDelta.y = m_BandHeight * (0.3f + 0.7f * open);
			}
			if (m_BannerTime >= total)
			{
				UI::SetVisible(m_Banner, false);
				m_BannerTime = -1.0f;
			}
		}

		// An entity in this canvas's subtree by its name.
		Entity Child(Scene& scene, const std::string& name)
		{
			std::vector<Entity> subtree;
			scene.GetSubtree(GetEntity(), subtree);
			for (const Entity& entity : subtree)
			{
				if (entity.GetName() == name)
				{
					return entity;
				}
			}
			HY_WARN("QuestTracker: '{}' has no '{}' (rebuild the field UI with scripts/ui/build_game_ui.py)", GetEntity().GetName(), name);
			return {};
		}

		Entity m_Tracker, m_TrackerTitle, m_TrackerObjective, m_TrackerCount;
		Entity m_Banner, m_BannerHeader, m_BannerTitle, m_BannerBand;
		Entity m_BannerRules[2];
		float m_RuleWidth[2] = { 0.0f, 0.0f };
		float m_BandHeight = 0.0f;
		QuestUIFade m_TrackerFade;
		QuestUIFade m_BannerFade;
		glm::vec4 m_TitleColor{ 1.0f };
		uint32_t m_Revision = 0;
		float m_TrackerAlpha = 0.0f;
		float m_TrackerX = 0.0f;
		bool m_TrackerSlide = false;
		float m_Flash = 0.0f;
		float m_BannerTime = -1.0f; // below 0: no banner
	};
}
