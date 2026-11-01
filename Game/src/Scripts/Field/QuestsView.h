// Game/src/Scripts/Field/QuestsView.h
#pragma once

#include "Hydro.h"
#include "Data/QuestDefinition.h"
#include "State/QuestLog.h"
#include "Views/ViewHelpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The field menu's Quests page (the Menu plan's M4; "Page Quests" in "Field Menu Canvas": rows "Quest 0" to
	// "Quest 9", the focused quest's details and steps; scripts/ui/build_game_ui.py builds it). Owned by FieldMenuView.
	// It shows the run's QuestLog: unfinished quests first, then finished ones ("Done"), newest first in each; the
	// focus selects one, and the right shows its title, state, current objective (with its count) and the steps so far.
	class QuestsView
	{
	public:
		static constexpr int MaxRows = 10;  // the rows in the scene; more quests aren't listed
		static constexpr int MaxSteps = 8;  // the step lines

		enum class Request : uint8_t { None, Back };

		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			const char* owner = "Quests page";
			for (int i = 0; i < MaxRows; i++)
			{
				m_Row[i] = ViewHelpers::Find(scene, "Quest " + std::to_string(i), owner);
				m_Name[i] = ViewHelpers::Find(scene, "Quest " + std::to_string(i) + " Name", owner);
				m_State[i] = ViewHelpers::Find(scene, "Quest " + std::to_string(i) + " Count", owner);
			}
			for (int i = 0; i < MaxSteps; i++)
			{
				m_Steps[i] = ViewHelpers::Find(scene, "Quest Step " + std::to_string(i), owner);
			}
			m_Count = ViewHelpers::Find(scene, "Quest Header Count", owner);
			m_Title = ViewHelpers::Find(scene, "Quest Detail Title", owner);
			m_Status = ViewHelpers::Find(scene, "Quest Detail State", owner);
			m_Objective = ViewHelpers::Find(scene, "Quest Detail Objective", owner);
			m_ObjectiveLabel = ViewHelpers::Find(scene, "Quest Detail Objective Label", owner);
			m_StepsLabel = ViewHelpers::Find(scene, "Quest Detail Steps Label", owner);
		}

		bool HasQuests() const
		{
			return m_Order.empty() == false;
		}

		// Moves the focus into the list, on the selection.
		void Focus()
		{
			m_Selected = std::clamp(m_Selected, 0, std::max(0, (int)m_Order.size() - 1));
			UI::Focus(EntityOf(m_Row[m_Selected]));
		}

		// A row's events: the focus selects, Cancel asks to go back to the tabs. Returns whether the event was a row's.
		bool HandleEvent(const UIEvent& event, Request& request)
		{
			request = Request::None;
			for (int i = 0; i < MaxRows; i++)
			{
				if ((entt::entity)event.Target != m_Row[i])
				{
					continue;
				}
				if (event.Type == UIEventType::FocusEnter)
				{
					m_Selected = i;
					PresentDetails();
				}
				else if (event.Type == UIEventType::Cancel)
				{
					request = Request::Back;
				}
				return true;
			}
			return false;
		}

		// The list (rows past it hidden) and the selected quest's details.
		void Present()
		{
			if (m_Scene == nullptr)
			{
				return;
			}
			const QuestLog& log = QuestLog::Get();
			m_Order.clear();
			int active = 0;
			for (int pass = 0; pass < 2; pass++)
			{
				for (int q = (int)log.Quests.size() - 1; q >= 0; q--)
				{
					if (log.Quests[(size_t)q].Done == (pass == 1) && (int)m_Order.size() < MaxRows)
					{
						m_Order.push_back(q);
						active += pass == 0 ? 1 : 0;
					}
				}
			}
			for (int i = 0; i < MaxRows; i++)
			{
				const bool used = i < (int)m_Order.size();
				ViewHelpers::SetVisible(EntityOf(m_Row[i]), used);
				if (used == false)
				{
					continue;
				}
				const QuestLog::Quest& quest = log.Quests[(size_t)m_Order[(size_t)i]];
				UI::SetText(EntityOf(m_Name[i]), quest.GetTitle());
				UI::SetText(EntityOf(m_State[i]), quest.Done ? "Done" : "");
				SetColor(m_Name[i], quest.Done ? Dim() : Cream());
			}
			UI::SetText(EntityOf(m_Count), std::to_string(active) + " active");
			PresentDetails();
		}

		// The selected quest's title, state, objective and steps (the stages up to the current one; finished ones dim).
		void PresentDetails()
		{
			const QuestLog& log = QuestLog::Get();
			if (m_Order.empty())
			{
				UI::SetText(EntityOf(m_Title), "No quests yet");
				UI::SetText(EntityOf(m_Status), "Talk to people to find out what needs doing.");
				UI::SetText(EntityOf(m_Objective), "");
				ViewHelpers::SetVisible(EntityOf(m_ObjectiveLabel), false);
				ViewHelpers::SetVisible(EntityOf(m_StepsLabel), false);
				for (int i = 0; i < MaxSteps; i++)
				{
					ViewHelpers::SetVisible(EntityOf(m_Steps[i]), false);
				}
				return;
			}
			m_Selected = std::clamp(m_Selected, 0, (int)m_Order.size() - 1);
			const QuestLog::Quest& quest = log.Quests[(size_t)m_Order[(size_t)m_Selected]];
			ViewHelpers::SetVisible(EntityOf(m_ObjectiveLabel), quest.Done == false);
			ViewHelpers::SetVisible(EntityOf(m_StepsLabel), true);
			UI::SetText(EntityOf(m_Title), quest.GetTitle());
			UI::SetText(EntityOf(m_Status), quest.Done ? "Complete" : "In progress");
			std::string objective = quest.Done ? std::string() : quest.GetObjective();
			if (quest.Done == false && quest.Goal > 0)
			{
				objective += " (" + std::to_string(std::min(quest.Progress, quest.Goal)) + " / " + std::to_string(quest.Goal) + ")";
			}
			UI::SetText(EntityOf(m_Objective), objective);

			const DataView<QuestDefinition> definition = Data::GetRow<QuestDefinition>(quest.Definition);
			const int stages = definition.IsValid() ? (int)definition->Stages.size() : 0;
			const int shown = quest.Done ? stages : std::min(stages, quest.Stage + 1);
			for (int i = 0; i < MaxSteps; i++)
			{
				const bool used = i < shown;
				ViewHelpers::SetVisible(EntityOf(m_Steps[i]), used);
				if (used)
				{
					UI::SetText(EntityOf(m_Steps[i]), definition->Stages[(size_t)i].Objective);
					SetColor(m_Steps[i], quest.Done || i < quest.Stage ? Dim() : Cream());
				}
			}
		}

	private:
		static glm::vec4 Dim() { return { 0.74f, 0.71f, 0.64f, 1.0f }; }
		static glm::vec4 Cream() { return { 0.95f, 0.91f, 0.83f, 1.0f }; }

		void SetColor(entt::entity entity, const glm::vec4& color)
		{
			Entity text = EntityOf(entity);
			if (UITextComponent* component = text.IsValid() ? text.TryGetComponent<UITextComponent>() : nullptr)
			{
				component->Color = color;
			}
		}

		Entity EntityOf(entt::entity entity) const { return Entity{ entity, m_Scene }; }

		Scene* m_Scene = nullptr;
		entt::entity m_Row[MaxRows] = {};
		entt::entity m_Name[MaxRows] = {};
		entt::entity m_State[MaxRows] = {};
		entt::entity m_Steps[MaxSteps] = {};
		entt::entity m_Count = entt::null, m_Title = entt::null, m_Status = entt::null, m_Objective = entt::null;
		entt::entity m_ObjectiveLabel = entt::null, m_StepsLabel = entt::null;
		std::vector<int> m_Order; // the rows' quests (indices into QuestLog::Quests)
		int m_Selected = 0;
	};
}
