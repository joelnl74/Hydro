// Game/src/Scripts/ShrineGlade/ShrineQuestScript.h
#pragma once

#include "Hydro.h"
#include "Data/DialogueDefinition.h"
#include "Data/QuestDefinition.h"
#include "Scripts/Common/DioramaCamera.h"
#include "Scripts/Field/EnemyChaseScript.h"
#include "State/QuestLog.h"
#include "Views/DialogueScript.h"
#include "Scripts/ShrineGlade/TreeAwakening.h"
#include "Views/LetterboxView.h"
#include "Views/QuestTrackerScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The Shrine Glade's opening errand, start to finish (docs/TestBed.md, "Shrine Glade quest"):
	//   1. Offer: a quest marker over the Old Monk ("Npc Elder"). Talking to him starts "Clearing the Grounds"
	//      and wakes the dormant slimes on the lower path (tags starting "Slime Grounds", EnemyChaseScript
	//      Dormant).
	//   2. Hunt: the tracker counts them as their battles are won (a won battle removes the slime).
	//   3. Return: the marker turns gold; talking to him plays the cutscene: letterbox bars ("Cinematic Canvas",
	//      which captures input, so Miko stands still), the camera glides to the Sacred Tree, the ground rumbles
	//      (camera shake) and stops dead, the tree wakes ("Sacred Tree Awake" and "Sacred Tree Glow" fade in, a
	//      cyan light and rising motes), then the monk's lines while the camera stays on the tree.
	//   4. The camera returns, "Clearing the Grounds" completes and "The Guardian's Duty" starts: down the shrine
	//      road into the rice fields, where CountrysideQuestScript takes it on. The tree stays awake.
	// The lines are data (DialogueTable, QuestTable); the timings and the camera are HY_PROPERTY fields,
	// tunable while playing.
	HY_SCRIPT()
	class ShrineQuestScript : public ScriptableEntity
	{
	public:
		// The Sandbox project's data files: the defaults below, so the scene didn't have to change.
		static constexpr uint64_t DefaultQuestTable = 5008924285669857658ull;    // Assets/Data/Quests.hytable
		static constexpr uint64_t DefaultDialogueTable = 8378147269873057234ull; // Assets/Data/Dialogue/ShrineErrand.hytable

		HY_PROPERTY(Asset(DataTable, QuestDefinition), Tooltip("The quests: rows ClearingTheGrounds and GuardiansDuty"))
		AssetHandle QuestTable = DefaultQuestTable;
		HY_PROPERTY(Asset(DataTable, DialogueDefinition), Tooltip("What is said: rows Intro, ReminderOne, ReminderMany ({Left}), Report, Awake, Farewell"))
		AssetHandle DialogueTable = DefaultDialogueTable;
		HY_PROPERTY(Tooltip("The NPC this quest speaks for: the name after \"Npc \" in its tag"))
		std::string MonkName = "Elder";
		HY_PROPERTY(Tooltip("Tag prefix of the quest's slimes"))
		std::string SlimePrefix = "Slime Grounds";

		// The cutscene's camera.
		HY_PROPERTY(Tooltip("Metres from the Sacred Tree's foot to where the camera looks, on the ground"))
		glm::vec2 TreeFocusOffset{ 0.8f, -0.5f };
		HY_PROPERTY(Range(0, 20), Tooltip("Metres above the tree's foot the camera looks at"))
		float TreeFocusRise = 4.5f;
		HY_PROPERTY(Range(0.3, 2), Tooltip("How much the camera sees at the end of the slow push in (1 = as in play)"))
		float TreeZoom = 0.82f;
		HY_PROPERTY(Range(0.1, 10), Tooltip("Seconds the camera takes to glide to the tree (and a little less back)"))
		float GlideTime = 2.4f;

		// The tremor.
		HY_PROPERTY(Range(0, 2), Tooltip("Metres the camera shakes at the tremor's peak"))
		float ShakeStrength = 0.22f;
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds into the cutscene the ground starts to move"))
		float TremorStart = 2.6f;
		HY_PROPERTY(Range(0.1, 30), Tooltip("Seconds the ground moves (it stops dead, not fading)"))
		float TremorTime = 4.2f;
		HY_PROPERTY(Range(0.1, 30), Tooltip("Seconds the tree takes to wake, from the tremor's start"))
		float AwakenTime = 4.5f;
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds of stillness after the tremor before the monk speaks"))
		float AfterTremor = 1.4f;
		HY_PROPERTY(Color)
		glm::vec3 GlowColor{ 0.35f, 0.9f, 1.0f };

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			// The log belongs to the run (the game session): a new run starts with an empty one, and a Shrine Glade
			// entered from another scene keeps it.
			m_Monk = Find(scene, "Npc " + MonkName);
			m_Player = Find(scene, "Player");
			m_Tree.Bind(scene, GlowColor, &Find);
			m_DialogueCanvas = Find(scene, "Dialogue Canvas");
			m_Letterbox.Bind(scene);
			m_MarkerCanvas = Find(scene, "Quest Marker Canvas");
			m_Marker = Find(scene, "Quest Marker");

			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag.rfind(SlimePrefix, 0) == 0 && scene.m_Registry.has<NativeScriptComponent>(entity))
				{
					m_Slimes.push_back(entity);
				}
			}

			m_Letterbox.SetShown(false);
			m_Letterbox.Set(0.0f);
			QuestLog::Get().Claim(MonkName);
			// Coming back to the glade after the errand: the tree is awake and the monk only says goodbye.
			if (QuestLog::Get().Find(DutyQuest()) != nullptr)
			{
				m_Step = Step::Done;
				m_Tree.CreateEffects(scene);
				m_Tree.Set(1.0f, 0.0f);
			}
			else
			{
				QuestLog::Get().Remove(SlimesQuest());
				m_Step = Step::Offer;
			}
			HY_INFO("Shrine quest: talk to the Old Monk on the shrine terrace (E)");
		}

		void OnDestroy() override
		{
			QuestLog::Get().Release(MonkName);
			DioramaView::ResetCinematic();
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			const bool talked = QuestLog::Get().TakeTalk(MonkName);
			UpdateConversation(scene);

			switch (m_Step)
			{
				case Step::Offer:
					if (talked)
					{
						Say("Intro");
						m_Step = Step::Briefing;
					}
					break;
				case Step::Briefing:
					if (IsTalking() == false)
					{
						WakeSlimes(scene);
						QuestLog::Get().Start(SlimesQuest(), m_SlimeCount); // stage 0: drive the slimes from the lower path
						m_Step = Step::Hunt;
					}
					break;
				case Step::Hunt:
				{
					const int left = CountSlimes(scene);
					QuestLog::Get().SetProgress(SlimesQuest(), m_SlimeCount - left);
					if (talked)
					{
						Say(left == 1 ? "ReminderOne" : "ReminderMany", std::to_string(left));
					}
					if (left == 0 && IsTalking() == false)
					{
						QuestLog::Get().SetStage(SlimesQuest(), 1); // return to the Old Monk
						m_Step = Step::Return;
					}
					break;
				}
				case Step::Return:
					if (talked)
					{
						Say("Report");
						m_Step = Step::Report;
					}
					break;
				case Step::Report:
					if (IsTalking() == false)
					{
						StartCutscene(scene);
					}
					break;
				case Step::Tremor:
					UpdateTremor(scene, deltaTime);
					break;
				case Step::Awakened:
					UpdateAwakeTree(scene, deltaTime);
					if (IsTalking() == false)
					{
						m_StepTime = 0.0f;
						m_Step = Step::Release;
					}
					break;
				case Step::Release:
					UpdateAwakeTree(scene, deltaTime);
					UpdateRelease(deltaTime);
					break;
				case Step::Done:
					UpdateAwakeTree(scene, deltaTime);
					if (talked)
					{
						Say("Farewell");
					}
					break;
			}

			UpdateMarker(scene);
		}

	private:
		enum class Step : uint8_t { Offer, Briefing, Hunt, Return, Report, Tremor, Awakened, Release, Done };

		// The quests (rows of QuestTable).
		DataRowRef SlimesQuest() const { return DataRowRef{ QuestTable, "ClearingTheGrounds" }; }
		DataRowRef DutyQuest() const { return DataRowRef{ QuestTable, "GuardiansDuty" }; }

		// ---- Conversations: a row of DialogueTable in the dialogue box ------------------------------------

		// Queues the conversation `row`; "{Left}" in its lines becomes `left` (the slimes still to find).
		void Say(const char* row, const std::string& left = {})
		{
			const DataView<DialogueDefinition> dialogue = Data::GetRow<DialogueDefinition>(DataRowRef{ DialogueTable, row });
			if (dialogue.IsValid() == false)
			{
				return; // warned once by Data::GetRow
			}
			m_Lines = DialogueScript::GetLines(*dialogue);
			for (DialogueScript::Line& line : m_Lines)
			{
				for (size_t at = line.Text.find("{Left}"); at != std::string::npos; at = line.Text.find("{Left}", at + left.size()))
				{
					line.Text.replace(at, 6, left);
				}
			}
			m_Waiting = true;
		}

		// True while a conversation waits or the dialogue box is open.
		bool IsTalking()
		{
			DialogueScript* dialogue = FindDialogue();
			return m_Waiting || (dialogue != nullptr && dialogue->IsOpen());
		}

		void UpdateConversation([[maybe_unused]] Scene& scene)
		{
			if (m_Waiting == false)
			{
				return;
			}
			DialogueScript* dialogue = FindDialogue();
			if (dialogue == nullptr)
			{
				// A scene without the dialogue box: the lines go to the log.
				for (const DialogueScript::Line& line : m_Lines)
				{
					HY_INFO("{}: {}", line.Speaker, line.Text);
				}
				m_Waiting = false;
				return;
			}
			if (dialogue->IsOpen() == false)
			{
				dialogue->Open(m_Lines);
				m_Waiting = false;
			}
		}

		DialogueScript* FindDialogue()
		{
			if (m_DialogueCanvas.IsValid() == false)
			{
				return nullptr;
			}
			return m_DialogueCanvas.TryGetComponent<DialogueScript>();
		}

		// ---- The slimes ---------------------------------------------------------------------------------------

		void WakeSlimes(Scene& scene)
		{
			m_SlimeCount = 0;
			for (entt::entity slime : m_Slimes)
			{
				if (scene.m_Registry.valid(slime) == false)
				{
					continue;
				}
				m_SlimeCount++;
				Entity slimeEntity{ slime, &scene };
				if (EnemyChaseScript* enemy = slimeEntity.TryGetComponent<EnemyChaseScript>())
				{
					enemy->Wake();
				}
			}
			if (m_SlimeCount == 0)
			{
				HY_WARN("Shrine quest: no slimes tagged \"{}...\" in the scene (scripts/quests/build_shrine_quest.py adds them)", SlimePrefix);
			}
		}

		// The slimes still there (a won battle destroys the slime that started it).
		int CountSlimes(Scene& scene) const
		{
			int left = 0;
			for (entt::entity slime : m_Slimes)
			{
				if (scene.m_Registry.valid(slime))
				{
					left++;
				}
			}
			return left;
		}

		// ---- The cutscene --------------------------------------------------------------------------------

		void StartCutscene(Scene& scene)
		{
			m_Step = Step::Tremor;
			m_StepTime = 0.0f;
			m_Letterbox.SetShown(true);
			// Where the camera looks: the tree's foot (an Upright sprite stands on its bottom edge), on its terrace.
			const glm::vec3 foot = m_Tree.GetFoot(scene);
			DioramaView::CinematicFocus = glm::vec2(foot) + TreeFocusOffset;
			DioramaView::CinematicHeight = foot.z + TreeFocusRise;
			DioramaView::CinematicWeight = 0.0f;
			DioramaView::CinematicZoom = 1.0f;
			DioramaView::CinematicShake = 0.0f;
			m_Tree.CreateEffects(scene);
			HY_INFO("Shrine quest: the tremor");
		}

		void UpdateTremor(Scene& scene, float deltaTime)
		{
			m_StepTime += deltaTime;
			const float t = m_StepTime;
			m_Letterbox.Set(Smooth(t / 0.8f));
			DioramaView::CinematicWeight = Smooth(t / GlideTime);

			// The ground moves: it builds over a second, judders at the peak, then stops dead.
			const float tremor = t - TremorStart;
			float shake = 0.0f;
			if (tremor >= 0.0f && tremor < TremorTime)
			{
				const float build = Smooth(tremor / 1.0f);
				const float surge = 0.75f + 0.25f * std::sin(tremor * 9.0f) * std::sin(tremor * 2.3f);
				shake = ShakeStrength * build * surge;
			}
			DioramaView::CinematicShake = shake;
			if (tremor >= 0.0f && m_LeavesFallen == false)
			{
				m_LeavesFallen = true;
				m_Tree.ShakeLoose(scene);
			}

			// The slow push in, and the tree waking from the tremor's start.
			DioramaView::CinematicZoom = 1.0f + (TreeZoom - 1.0f) * Smooth((t - 1.0f) / (TremorStart + AwakenTime));
			const float awake = Smooth(tremor / AwakenTime);
			m_Tree.Set(awake, m_Time);

			if (tremor >= TremorTime + AfterTremor)
			{
				DioramaView::CinematicShake = 0.0f;
				Say("Awake");
				m_Step = Step::Awakened;
				HY_INFO("Shrine quest: the Sacred Tree is awake");
			}
		}

		// The tree once awake: its veins and light breathe slowly.
		void UpdateAwakeTree([[maybe_unused]] Scene& scene, [[maybe_unused]] float deltaTime)
		{
			m_Tree.Set(1.0f, m_Time);
		}

		void UpdateRelease(float deltaTime)
		{
			m_StepTime += deltaTime;
			const float back = Smooth(m_StepTime / (GlideTime * 0.7f));
			DioramaView::CinematicWeight = 1.0f - back;
			DioramaView::CinematicZoom = TreeZoom + (1.0f - TreeZoom) * back;
			m_Letterbox.Set(1.0f - Smooth(m_StepTime / 0.8f));
			if (back >= 1.0f)
			{
				DioramaView::ResetCinematic();
				m_Letterbox.Set(0.0f);
				m_Letterbox.SetShown(false);
				QuestLog::Get().Complete(SlimesQuest());
				QuestLog::Get().Start(DutyQuest()); // down the shrine road into the rice fields
				m_Step = Step::Done;
			}
		}

		// ---- The quest marker over the monk -------------------------------------------------------------------

		void UpdateMarker(Scene& scene)
		{
			const bool offer = m_Step == Step::Offer;
			const bool report = m_Step == Step::Return;
			const bool shown = (offer || report) && UI::CapturesInput(scene) == false && m_Monk.IsValid();
			if (UI::IsVisible(m_MarkerCanvas) != shown)
			{
				UI::SetVisible(m_MarkerCanvas, shown);
			}
			if (shown == false || m_MarkerCanvas.IsValid() == false)
			{
				return;
			}
			// Over the monk's head, bobbing a little: vermilion for a new errand, gold for "come back to me".
			const glm::vec3 foot = TreeAwakening::Foot(scene, m_Monk);
			const float bob = 0.08f * std::sin(m_Time * 3.0f);
			scene.SetWorldPosition(m_MarkerCanvas, DioramaView::Above(foot, 2.35f + bob));
			if (m_Marker.IsValid() && m_Marker.HasComponent<UIImageComponent>())
			{
				m_Marker.GetComponent<UIImageComponent>().Color = offer ? glm::vec4(1.0f, 0.42f, 0.3f, 1.0f) : glm::vec4(1.0f, 0.84f, 0.42f, 1.0f);
			}
		}

		// ---- Helpers -------------------------------------------------------------------------------------

		static float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		static Entity Find(Scene& scene, const std::string& tag)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == tag)
				{
					return Entity{ entity, &scene };
				}
			}
			HY_WARN("Shrine quest: the scene has no entity tagged \"{}\"", tag);
			return {};
		}

		Step m_Step = Step::Offer;
		float m_Time = 0.0f;
		float m_StepTime = 0.0f;
		std::vector<DialogueScript::Line> m_Lines; // the conversation waiting for the dialogue box
		bool m_Waiting = false;
		Entity m_Monk, m_Player;
		Entity m_DialogueCanvas, m_MarkerCanvas, m_Marker;
		TreeAwakening m_Tree;    // the Sacred Tree waking (Scripts/ShrineGlade/TreeAwakening.h)
		LetterboxView m_Letterbox; // the cutscene bars (Views/LetterboxView.h)
		std::vector<entt::entity> m_Slimes;
		int m_SlimeCount = 0;
		bool m_LeavesFallen = false;
	};
}
