#pragma once

#include "Hydro.h"
#include "State/PartyState.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace Hydro
{
	// The field menu's Party page: the line-up (who fights, at most PartyState::GetLineupLimit()) on the left, the
	// reserve on the right, the focused member's numbers below. Enter moves a member from one to the other.
	class PartyView
	{
	public:
		static constexpr int LineupRows = PartyState::MaxFighters;
		static constexpr int ReserveRows = 8;

		enum class Request : uint8_t { None, Back, Changed };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Party page";
			for (int i = 0; i < LineupRows; i++)
			{
				m_Lineup[(size_t)i] = BindRow(scene, "Lineup Member " + std::to_string(i), owner);
			}
			for (int i = 0; i < ReserveRows; i++)
			{
				m_Reserve[(size_t)i] = BindRow(scene, "Reserve Member " + std::to_string(i), owner);
			}
			m_LineupCount = ViewHelpers::Find(scene, "Party Lineup Panel Header Count", owner);
			m_ReserveCount = ViewHelpers::Find(scene, "Party Reserve Panel Header Count", owner);
			m_DetailName = ViewHelpers::Find(scene, "Party Detail Name", owner);
			m_DetailInfo = ViewHelpers::Find(scene, "Party Detail Info", owner);
			m_DetailText = ViewHelpers::Find(scene, "Party Detail Text", owner);
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
		}

		// On the member focused last (or the first of the line-up).
		void Focus()
		{
			FocusMember(std::clamp(m_Member, 0, std::max(0, PartyState::Get().GetSize() - 1)));
		}

		// Returns whether the event was the page's; Changed when a member moved (the status column follows).
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			const int member = MemberOfRow((entt::entity)event.Target);
			if (member == NoRow)
			{
				return false;
			}
			switch (event.Type)
			{
			case UIEventType::FocusEnter:
				m_Member = member;
				PresentDetail();
				break;
			case UIEventType::Submit:
				if (member >= 0 && Move(member))
				{
					request = Request::Changed;
				}
				break;
			case UIEventType::Cancel:
				request = Request::Back;
				break;
			default:
				break;
			}
			return true;
		}

		// What Enter does on the focused member, for the key bar.
		const char* GetEnterKey() const
		{
			const PartyState& party = PartyState::Get();
			if (party.GetSize() == 0)
			{
				return "";
			}
			return party.GetMember(m_Member).Reserve ? "Enter  Add        " : "Enter  Remove        ";
		}

		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const PartyState& party = PartyState::Get();
			const int lineup = party.GetLineupSize();
			const int limit = PartyState::GetLineupLimit();
			// The line-up's empty places down to its limit show as "-"; rows past the limit are hidden.
			for (int i = 0; i < LineupRows; i++)
			{
				PresentRow(m_Lineup[(size_t)i], i < lineup ? i : -1, i < std::max(lineup, limit));
			}
			for (int i = 0; i < ReserveRows; i++)
			{
				const int member = lineup + i;
				PresentRow(m_Reserve[(size_t)i], member < party.GetSize() ? member : -1, member < party.GetSize());
			}
			UI::SetText(EntityOf(m_LineupCount), std::to_string(lineup) + " / " + std::to_string(limit));
			UI::SetText(EntityOf(m_ReserveCount), std::to_string(party.GetSize() - lineup));
			if (party.GetSize() - lineup > ReserveRows)
			{
				HY_WARN("Party page: {} members in reserve, the page shows {}", party.GetSize() - lineup, ReserveRows);
			}
			PresentDetail();
		}

	private:
		static constexpr int NoRow = -2; // not one of the page's rows (-1: an empty place)

		struct Row
		{
			entt::entity Button = entt::null, Name = entt::null, Detail = entt::null;
		};

		static Row BindRow(Scene& scene, const std::string& tag, const char* owner)
		{
			Row row;
			row.Button = ViewHelpers::Find(scene, tag, owner);
			row.Name = ViewHelpers::Find(scene, tag + " Name", owner);
			row.Detail = ViewHelpers::Find(scene, tag + " Count", owner);
			return row;
		}

		// The member a row shows (an index into PartyState::Members), -1 for an empty place, NoRow for other entities.
		int MemberOfRow(entt::entity target) const
		{
			const PartyState& party = PartyState::Get();
			const int lineup = party.GetLineupSize();
			for (int i = 0; i < LineupRows; i++)
			{
				if (target == m_Lineup[(size_t)i].Button)
				{
					return i < lineup ? i : -1;
				}
			}
			for (int i = 0; i < ReserveRows; i++)
			{
				if (target == m_Reserve[(size_t)i].Button)
				{
					return lineup + i < party.GetSize() ? lineup + i : -1;
				}
			}
			return NoRow;
		}

		// Out of the line-up into the reserve or back; the focus follows the member to their new row.
		bool Move(int member)
		{
			PartyState& party = PartyState::Get();
			const std::string hero = party.GetMember(member).Hero;
			const bool toReserve = party.GetMember(member).Reserve == false;
			if (party.SetReserve(member, toReserve) == false)
			{
				PresentDetail();
				return false;
			}
			m_Member = party.IndexOf(hero);
			Present();
			FocusMember(m_Member);
			return true;
		}

		void FocusMember(int member)
		{
			const PartyState& party = PartyState::Get();
			const int lineup = party.GetLineupSize();
			if (member < lineup)
			{
				UI::Focus(EntityOf(m_Lineup[(size_t)std::clamp(member, 0, LineupRows - 1)].Button));
			}
			else
			{
				UI::Focus(EntityOf(m_Reserve[(size_t)std::clamp(member - lineup, 0, ReserveRows - 1)].Button));
			}
		}

		void PresentRow(const Row& row, int member, bool shown)
		{
			Entity button = EntityOf(row.Button);
			ViewHelpers::SetVisible(button, shown);
			UI::SetInteractable(button, member >= 0);
			if (member < 0)
			{
				UI::SetText(EntityOf(row.Name), "-");
				UI::SetText(EntityOf(row.Detail), "");
				return;
			}
			const PartyMember& hero = PartyState::Get().GetMember(member);
			UI::SetText(EntityOf(row.Name), hero.GetStats().Name);
			UI::SetText(EntityOf(row.Detail), "Lv " + std::to_string(hero.Level));
		}

		// The focused member's level, HP and MP, and what Enter would do (or why it can't).
		void PresentDetail()
		{
			const PartyState& party = PartyState::Get();
			if (party.GetSize() == 0)
			{
				return;
			}
			const PartyMember& member = party.GetMember(m_Member);
			const HeroStats stats = member.GetStats();
			UI::SetText(EntityOf(m_DetailName), stats.Name);
			UI::SetText(EntityOf(m_DetailInfo), "Lv " + std::to_string(member.Level) + "   HP " + std::to_string(member.HP) + " / "
				+ std::to_string(stats.MaxHP) + "   MP " + std::to_string(member.MP) + " / " + std::to_string(stats.MaxMP));
			const int share = (int)std::lround(100.0f * std::clamp(BattleData::Party().ReserveExperience, 0.0f, 1.0f));
			std::string text;
			if (member.Reserve)
			{
				text = party.IsLineupFull()
					? "In reserve. The line-up is full (" + std::to_string(PartyState::GetLineupLimit()) + "): remove someone first."
					: "In reserve: waits out of battle and gets " + std::to_string(share) + "% of the experience. Enter adds them.";
			}
			else
			{
				text = party.GetLineupSize() <= 1 ? std::string("In the line-up. The last one in it stays.")
					: std::string("In the line-up: fights in battle. Enter moves them to the reserve.");
			}
			UI::SetText(EntityOf(m_DetailText), text);
			UI::SetText(EntityOf(m_Help), "Choose who fights: up to " + std::to_string(PartyState::GetLineupLimit()) + " in the line-up.");
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<Row, LineupRows> m_Lineup;
		std::array<Row, ReserveRows> m_Reserve;
		entt::entity m_LineupCount = entt::null, m_ReserveCount = entt::null;
		entt::entity m_DetailName = entt::null, m_DetailInfo = entt::null, m_DetailText = entt::null;
		entt::entity m_Help = entt::null;
		int m_Member = 0; // the focused member: an index into PartyState::Members
	};
}
