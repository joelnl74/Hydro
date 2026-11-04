#pragma once

#include "Hydro.h"
#include "State/Inventory.h"
#include "State/SaveGame.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <array>
#include <string>

namespace Hydro
{
	// The field menu's System page: save, load, restart the run, delete the save, and what the save slot holds. Everything
	// but a first save asks for Enter a second time. The director does the saving and the scene switches.
	class SystemView
	{
	public:
		enum class Option : uint8_t { Save = 0, Load, Restart, Delete };
		static constexpr int OptionCount = 4;

		enum class Request : uint8_t { None, Back, Save, Load, Restart };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "System page";
			for (int i = 0; i < OptionCount; i++)
			{
				m_Options[(size_t)i] = ViewHelpers::Find(scene, "System Option " + std::to_string(i), owner);
				m_OptionNames[(size_t)i] = ViewHelpers::Find(scene, "System Option " + std::to_string(i) + " Name", owner);
			}
			m_SlotInfo = ViewHelpers::Find(scene, "System Slot Info", owner);
			m_DetailText = ViewHelpers::Find(scene, "System Detail Text", owner);
			m_Help = ViewHelpers::Find(scene, "Help Text", owner);
		}

		void Focus()
		{
			m_Armed = -1;
			UI::Focus(EntityOf(m_Options[(size_t)std::clamp(m_Option, 0, OptionCount - 1)]));
		}

		// Shown under the options until the focus moves (what the last action did).
		void SetNotice(const std::string& notice)
		{
			m_Notice = notice;
			PresentDetail();
		}

		// Returns whether the event was the page's.
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			for (int i = 0; i < OptionCount; i++)
			{
				if ((entt::entity)event.Target != m_Options[(size_t)i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Option = i;
					m_Armed = -1;
					m_Notice.clear();
					PresentDetail();
					break;
				case UIEventType::Submit:
					request = Choose((Option)i);
					break;
				case UIEventType::Cancel:
					request = Request::Back;
					break;
				default:
					break;
				}
				return true;
			}
			return false;
		}

		// Reads the slot's summary again (after a save or a delete, or when the menu opens).
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			m_Summary = SaveGame::ReadSummary();
			const char* names[OptionCount] = { "Save", "Load", "Restart", "Delete save" };
			for (int i = 0; i < OptionCount; i++)
			{
				UI::SetText(EntityOf(m_OptionNames[(size_t)i]), names[i]);
				const bool needsSave = i == (int)Option::Load || i == (int)Option::Delete;
				UI::SetInteractable(EntityOf(m_Options[(size_t)i]), needsSave == false || m_Summary.Exists);
			}
			if (m_Summary.Exists)
			{
				UI::SetText(EntityOf(m_SlotInfo), m_Summary.Saved + "\n" + m_Summary.Scene + "\n" + m_Summary.Party + "\n"
					+ std::to_string(m_Summary.Money) + " " + CurrencyName);
			}
			else
			{
				UI::SetText(EntityOf(m_SlotInfo), "Empty");
			}
			PresentDetail();
		}

	private:
		// A first press arms an option that loses or overwrites something; the second does it.
		Request Choose(Option option)
		{
			const bool confirm = option != Option::Save || m_Summary.Exists;
			if (confirm && m_Armed != (int)option)
			{
				m_Armed = (int)option;
				m_Notice.clear();
				PresentDetail();
				return Request::None;
			}
			m_Armed = -1;
			switch (option)
			{
			case Option::Save:
				return Request::Save;
			case Option::Load:
				return Request::Load;
			case Option::Restart:
				return Request::Restart;
			case Option::Delete:
				SetNotice(SaveGame::Delete() ? "The save was deleted." : "There was no save to delete.");
				Present();
				return Request::None;
			}
			return Request::None;
		}

		void PresentDetail()
		{
			static const char* what[OptionCount] = {
				"Write the party, items, quests and where you stand to the save slot.",
				"Go back to the saved game. Progress since then is lost.",
				"Start a new game from the beginning. Progress that isn't saved is lost.",
				"Delete the save slot." };
			static const char* sure[OptionCount] = {
				"Press Enter again to overwrite the save.",
				"Press Enter again to load the save.",
				"Press Enter again to restart.",
				"Press Enter again to delete the save." };
			const int option = std::clamp(m_Option, 0, OptionCount - 1);
			std::string text = m_Armed == option ? sure[option] : what[option];
			if (m_Notice.empty() == false)
			{
				text = m_Notice;
			}
			UI::SetText(EntityOf(m_DetailText), text);
			UI::SetText(EntityOf(m_Help), "Save, load or start over.");
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		std::array<entt::entity, OptionCount> m_Options = {};
		std::array<entt::entity, OptionCount> m_OptionNames = {};
		entt::entity m_SlotInfo = entt::null, m_DetailText = entt::null, m_Help = entt::null;
		SaveGame::Summary m_Summary;
		std::string m_Notice;
		int m_Option = 0; // the focused option
		int m_Armed = -1; // the option waiting for its second Enter
	};
}
