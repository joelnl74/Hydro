// Game/src/Scripts/Field/QuestGiverScript.h
#pragma once

#include "Hydro.h"
#include "Data/QuestDefinition.h"
#include "Rules/QuestRules.h"
#include "Scripts/Common/DioramaCamera.h"
#include "State/BattleState.h"
#include "State/Inventory.h"
#include "State/QuestLog.h"
#include "Views/DialogueScript.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace Hydro
{
	// The side quests of a field scene (Nov 2, 2026): every row of the quests table whose Kind is Defeat or Collect and
	// whose Giver stands in this scene ("Npc <Giver>") is offered by that NPC, one at a time in row order. Talking to a
	// giver: the offer (OfferLines; the quest starts when the box closes), how many are left (ProgressLines, {Left}),
	// or, once it is done, the hand-in: the collected items are handed over, the rewards given (items, aster,
	// experience, listed after DoneLines) and the quest completes. A giver with nothing left to offer talks as usual
	// (Npcs.hytable). The counting is QuestRules: battles record defeats (BattleScript), Collect quests follow the
	// inventory every frame. A marker floats over a giver ("Quest Marker <Giver>", a World canvas the scene's builder
	// adds): vermilion with a quest to offer, gold when one can be turned in. Put it once in a scene, on any entity.
	HY_SCRIPT()
	class QuestGiverScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultQuestTable = 5008924285669857658ull; // Assets/Data/Quests.hytable

		HY_PROPERTY(Asset(DataTable, QuestDefinition), Tooltip("The quests; its Defeat and Collect rows whose giver is in this scene are offered"))
		AssetHandle QuestTable = DefaultQuestTable;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_DialogueCanvas = scene.FindEntityByTag("Dialogue Canvas");
			const DataTableView<QuestDefinition> table = Data::GetTable<QuestDefinition>(QuestTable);
			for (const auto& [id, quest] : table.Rows())
			{
				if (quest.Kind == QuestKind::Story || quest.Giver.empty())
				{
					continue;
				}
				Entity npc = scene.FindEntityByTag("Npc " + quest.Giver);
				if (npc.IsValid() == false)
				{
					continue; // a giver of another scene
				}
				Giver& giver = m_Givers[quest.Giver];
				giver.Npc = npc;
				giver.Marker = scene.FindEntityByTag("Quest Marker " + quest.Giver);
				giver.Image = scene.FindEntityByTag("Quest Marker " + quest.Giver + " Image");
				giver.Quests.push_back(id);
			}
			for (auto& [name, giver] : m_Givers)
			{
				QuestLog::Get().Claim(name);
				HY_INFO("Side quests: {} offers {} quest(s)", name, giver.Quests.size());
			}
		}

		void OnDestroy() override
		{
			for (auto& [name, giver] : m_Givers)
			{
				QuestLog::Get().Release(name);
			}
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			const Inventory& inventory = Inventory::Get();
			QuestLog::Get().RefreshCollect([&inventory](const DataRowRef& item) { return inventory.CountOf(item); });
			UpdateConversation();

			for (auto& [name, giver] : m_Givers)
			{
				const std::string current = CurrentFor(giver);
				if (QuestLog::Get().TakeTalk(name))
				{
					Talk(name, current);
				}
				if (current.empty() && giver.Released == false)
				{
					// Nothing left to offer: the NPC talks as usual from now on.
					QuestLog::Get().Release(name);
					giver.Released = true;
				}
				PresentMarker(scene, giver, current);
			}

			// The offer's box closed: the quest starts.
			if (m_PendingStart.empty() == false && IsTalking() == false)
			{
				const QuestDefinition* quest = Find(m_PendingStart);
				QuestLog::Get().Start(Ref(m_PendingStart), quest != nullptr ? quest->Count : 0);
				m_PendingStart.clear();
			}
		}

	private:
		struct Giver
		{
			Entity Npc, Marker, Image;
			std::vector<std::string> Quests; // their rows, in table order
			bool Released = false;
		};

		DataRowRef Ref(const std::string& row) const { return DataRowRef{ QuestTable, row }; }

		const QuestDefinition* Find(const std::string& row) const
		{
			return QuestLog::FindDefinition(Ref(row));
		}

		std::string CurrentFor(const Giver& giver) const
		{
			return QuestRules::CurrentFor(giver.Quests, [this](const std::string& row) { return QuestLog::Get().Find(Ref(row)); });
		}

		void Talk(const std::string& name, const std::string& row)
		{
			const QuestDefinition* quest = row.empty() ? nullptr : Find(row);
			if (quest == nullptr)
			{
				return;
			}
			QuestLog::Quest* entry = QuestLog::Get().Find(Ref(row));
			if (entry == nullptr)
			{
				Say(name, quest->OfferLines, {});
				m_PendingStart = row;
				return;
			}
			if (QuestRules::IsReady(*entry) == false)
			{
				Say(name, quest->ProgressLines, std::to_string(std::max(0, entry->Goal - entry->Progress)));
				return;
			}
			TurnIn(name, row, *quest);
		}

		// Hands the quest in: the collected items go, the rewards come, the lines say so.
		void TurnIn(const std::string& name, const std::string& row, const QuestDefinition& quest)
		{
			Inventory& inventory = Inventory::Get();
			if (quest.Kind == QuestKind::Collect && inventory.Remove(quest.Item, quest.Count) == false)
			{
				return; // spent meanwhile (RefreshCollect sends it back to collecting next frame)
			}
			std::vector<DialogueScript::Line> rewards;
			for (const InventoryEntry& reward : quest.RewardItems)
			{
				if (reward.Item.IsSet() == false || reward.Count <= 0)
				{
					continue;
				}
				inventory.Add(reward.Item, reward.Count);
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(reward.Item);
				const std::string itemName = item.IsValid() ? item->Name : reward.Item.Row;
				rewards.push_back({ std::string(), "Received " + itemName + (reward.Count > 1 ? " x" + std::to_string(reward.Count) : std::string()) + "." });
			}
			if (quest.RewardMoney > 0)
			{
				inventory.Money += quest.RewardMoney;
				rewards.push_back({ std::string(), "Received " + std::to_string(quest.RewardMoney) + " " + CurrencyName + "." });
			}
			if (quest.RewardExperience > 0)
			{
				rewards.push_back({ std::string(), "Gained " + std::to_string(quest.RewardExperience) + " experience." });
				for (const std::string& line : BattleState::Get().GainExperience(quest.RewardExperience))
				{
					rewards.push_back({ std::string(), line });
				}
			}
			Say(name, quest.DoneLines, {});
			m_Lines.insert(m_Lines.end(), rewards.begin(), rewards.end());
			QuestLog::Get().Complete(Ref(row));
			HY_INFO("Side quests: '{}' turned in to {}", quest.Title, name);
		}

		// ---- The marker over a giver ----------------------------------------------------------------------

		void PresentMarker(Scene& scene, const Giver& giver, const std::string& current)
		{
			const QuestLog::Quest* entry = current.empty() ? nullptr : QuestLog::Get().Find(Ref(current));
			const bool offer = current.empty() == false && entry == nullptr;
			const bool ready = entry != nullptr && QuestRules::IsReady(*entry);
			const bool shown = (offer || ready) && giver.Npc.IsValid() && UI::CapturesInput(scene) == false;
			if (giver.Marker.IsValid() == false)
			{
				return;
			}
			if (UI::IsVisible(giver.Marker) != shown)
			{
				UI::SetVisible(giver.Marker, shown);
			}
			if (shown == false)
			{
				return;
			}
			const float bob = 0.08f * std::sin(m_Time * 3.0f);
			scene.SetWorldPosition(giver.Marker, DioramaView::Above(scene.GetWorldPosition(giver.Npc), 2.35f + bob));
			Entity image = giver.Image;
			if (image.IsValid() && image.HasComponent<UIImageComponent>())
			{
				image.GetComponent<UIImageComponent>().Color = ready ? glm::vec4(1.0f, 0.84f, 0.42f, 1.0f) : glm::vec4(1.0f, 0.42f, 0.3f, 1.0f);
			}
		}

		// ---- Conversations ----------------------------------------------------------------------------------

		void Say(const std::string& speaker, const std::vector<DialogueLine>& lines, const std::string& left)
		{
			m_Lines.clear();
			for (const DialogueLine& line : lines)
			{
				std::string text = line.Text;
				for (size_t at = text.find("{Left}"); at != std::string::npos; at = text.find("{Left}", at + left.size()))
				{
					text.replace(at, 6, left);
				}
				m_Lines.push_back({ line.Speaker.empty() ? speaker : line.Speaker, text });
			}
			m_Waiting = true;
		}

		bool IsTalking()
		{
			DialogueScript* dialogue = FindDialogue();
			return m_Waiting || (dialogue != nullptr && dialogue->IsOpen());
		}

		void UpdateConversation()
		{
			if (m_Waiting == false)
			{
				return;
			}
			DialogueScript* dialogue = FindDialogue();
			if (dialogue == nullptr)
			{
				for (const DialogueScript::Line& line : m_Lines)
				{
					HY_INFO("{}: {}", line.Speaker, line.Text);
				}
				m_Waiting = false;
				return;
			}
			if (dialogue->IsOpen())
			{
				return; // someone else's lines first
			}
			if (m_Lines.empty() == false)
			{
				dialogue->Open(m_Lines);
			}
			m_Waiting = false;
		}

		DialogueScript* FindDialogue()
		{
			return m_DialogueCanvas.IsValid() ? m_DialogueCanvas.TryGetComponent<DialogueScript>() : nullptr;
		}

		std::map<std::string, Giver> m_Givers;
		std::vector<DialogueScript::Line> m_Lines;
		std::string m_PendingStart;
		bool m_Waiting = false;
		float m_Time = 0.0f;
		Entity m_DialogueCanvas;
	};
}
