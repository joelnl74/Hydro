#pragma once
#include "Hydro.h"
#include "BattleScripts.h"
#include "DialogueScripts.h"
#include "DioramaCamera.h"
#include "Data/DialogueDefinition.h"
#include "Data/QuestDefinition.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// A simple quest system for the field scenes: prototype code. The quests' words are data (Assets/Data/Quests.hytable:
// titles and the objective of each stage; Assets/Data/Dialogue/ShrineErrand.hytable: the errand's conversations);
// what happens when is still code (a story / cutscene system can replace the Shrine Glade's director later).
//   - QuestLog: the quests of a run (a row of the quests table, its stage, a count), the banners waiting to be shown
//     and the NPCs a quest speaks for. Run state on the game session (RpgSession::Quests), like BattleState.
//   - QuestTrackerScript: on "Quest Canvas" (every field scene, scripts/ui/build_game_ui.py): the tracked quest,
//     top right, and the "New Quest" / "Quest Complete" banner across the middle of the screen, after Octopath
//     Traveler's story banners (decision D5: the rules, not the look).
//   - ShrineQuestScript: on "Quest Director" in the Shrine Glade (scripts/quests/build_shrine_quest.py): the Old
//     Monk's errand (clear the slimes from the lower path), and the cutscene when Miko reports back: the
//     tremor (camera shake), the Sacred Tree waking, the monk sending her down to the village.
namespace Hydro
{
	struct QuestLog
	{
		// The running session's (RpgSession::Quests, Scripting plan SC2): a new run starts with an empty log.
		static QuestLog& Get();

		struct Quest
		{
			DataRowRef Definition; // its row of the quests table: the title and the stages' objectives
			int Stage = 0;
			int Progress = 0;
			int Goal = 0;          // 0: the objective has no count
			bool Done = false;

			// From the data, every time (an edit to the table shows at once); the row ID when the row is gone.
			std::string GetTitle() const
			{
				const DataView<QuestDefinition> quest = Data::GetRow<QuestDefinition>(Definition);
				return quest.IsValid() ? quest->Title : Definition.Row;
			}

			std::string GetObjective() const
			{
				const DataView<QuestDefinition> quest = Data::GetRow<QuestDefinition>(Definition);
				return quest.IsValid() && Stage >= 0 && Stage < (int)quest->Stages.size() ? quest->Stages[(size_t)Stage].Objective : std::string();
			}
		};

		struct Announcement
		{
			std::string Header; // "New Quest", "Quest Complete"
			std::string Title;
		};

		std::vector<Quest> Quests;
		std::vector<Announcement> Announcements; // oldest first; QuestTrackerScript shows and removes them
		uint32_t Revision = 0;                   // bumped by every change (the tracker flashes)


		Quest* Find(const DataRowRef& definition)
		{
			for (Quest& quest : Quests)
			{
				if (quest.Definition == definition)
				{
					return &quest;
				}
			}
			return nullptr;
		}

		// Adds the quest at its first stage (or restarts it) and announces it.
		void Start(const DataRowRef& definition, int goal = 0)
		{
			Quest* quest = Find(definition);
			if (quest == nullptr)
			{
				Quests.push_back(Quest());
				quest = &Quests.back();
			}
			quest->Definition = definition;
			quest->Stage = 0;
			quest->Progress = 0;
			quest->Goal = goal;
			quest->Done = false;
			Announcements.push_back({ "New Quest", quest->GetTitle() });
			Revision++;
			HY_INFO("Quest: started '{}': {}", quest->GetTitle(), quest->GetObjective());
		}

		// Moves the quest on to stage `stage` (its objective in the quests table).
		void SetStage(const DataRowRef& definition, int stage, int goal = 0)
		{
			if (Quest* quest = Find(definition))
			{
				quest->Stage = stage;
				quest->Progress = 0;
				quest->Goal = goal;
				Revision++;
				HY_INFO("Quest: '{}': {}", quest->GetTitle(), quest->GetObjective());
			}
		}

		void SetProgress(const DataRowRef& definition, int progress)
		{
			Quest* quest = Find(definition);
			if (quest != nullptr && quest->Progress != progress)
			{
				quest->Progress = progress;
				Revision++;
				HY_INFO("Quest: '{}': {} / {}", quest->GetTitle(), progress, quest->Goal);
			}
		}

		void Complete(const DataRowRef& definition)
		{
			Quest* quest = Find(definition);
			if (quest != nullptr && quest->Done == false)
			{
				quest->Done = true;
				Announcements.push_back({ "Quest Complete", quest->GetTitle() });
				Revision++;
				HY_INFO("Quest: completed '{}'", quest->GetTitle());
			}
		}

		void Remove(const DataRowRef& definition)
		{
			Quests.erase(std::remove_if(Quests.begin(), Quests.end(), [&](const Quest& quest) { return quest.Definition == definition; }), Quests.end());
			Revision++;
		}

		// The quest the tracker shows: the newest that isn't done (nullptr when there is none).
		const Quest* GetTracked()
		{
			for (auto it = Quests.rbegin(); it != Quests.rend(); ++it)
			{
				if (it->Done == false)
				{
					return &*it;
				}
			}
			return nullptr;
		}

		// NPCs a quest speaks for (by the name after "Npc " in their tag). The field's interaction (MvpDirectorScript)
		// calls Talk when the player talks to one: true means the quest takes it (TakeTalk, its director, next
		// update), so the NPC's usual lines are not shown.
		void Claim(const std::string& npc)
		{
			if (std::find(m_Claimed.begin(), m_Claimed.end(), npc) == m_Claimed.end())
			{
				m_Claimed.push_back(npc);
			}
		}

		void Release(const std::string& npc)
		{
			m_Claimed.erase(std::remove(m_Claimed.begin(), m_Claimed.end(), npc), m_Claimed.end());
		}

		bool Talk(const std::string& npc)
		{
			if (std::find(m_Claimed.begin(), m_Claimed.end(), npc) == m_Claimed.end())
			{
				return false;
			}
			m_Talked = npc;
			return true;
		}

		// True once after the player talked to `npc` (and the quest claims it).
		bool TakeTalk(const std::string& npc)
		{
			if (m_Talked != npc)
			{
				return false;
			}
			m_Talked.clear();
			return true;
		}

	private:
		std::vector<std::string> m_Claimed;
		std::string m_Talked;
	};

	// The UI elements of a group faded together: each image's and text's own alpha is kept, and multiplied.
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

	// On "Quest Canvas" (a screen canvas that doesn't capture input): the quest tracker and the banners.
	//   - "Quest Tracker" (top right): "Quest Tracker Title", "Quest Tracker Objective", "Quest Tracker Count"; shown
	//     while a quest is tracked and no menu, dialogue or cutscene holds the screen (UI::CapturesInput); its
	//     title flashes gold when the quest changes.
	//   - "Quest Banner" (across the middle): "Quest Banner Header", "Quest Banner Title", "Quest Banner Band" and
	//     the two rules "Quest Banner Rule Top" / "Bottom", which open out from the middle; one announcement at a
	//     time, again only while nothing holds the screen (a quest completed in a cutscene is shown after it).
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
			UI::SetText(m_TrackerCount, quest->Goal > 0 ? std::to_string(quest->Progress) + " / " + std::to_string(quest->Goal) : std::string());
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
			// The rules open out from the middle and the band unfolds with them.
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

	// The Shrine Glade's opening errand, start to finish (docs/TestBed.md, "Shrine Glade quest"):
	//   1. Offer: a quest marker over the Old Monk ("Npc Elder"). Talking to him starts "Clearing the Grounds"
	//      and wakes the dormant slimes on the lower path (tags starting "Slime Grounds", EnemyChaseScript
	//      Dormant).
	//   2. Hunt: the tracker counts them as their battles are won (a won battle removes the slime).
	//   3. Return: the marker turns gold; talking to him plays the cutscene: letterbox bars ("Cinematic Canvas",
	//      which captures input, so Miko stands still), the camera glides to the Sacred Tree, the ground rumbles
	//      (camera shake) and stops dead, the tree wakes ("Sacred Tree Awake" and "Sacred Tree Glow" fade in, a
	//      cyan light and rising motes), then the monk's lines while the camera stays on the tree.
	//   4. The camera returns, "Clearing the Grounds" completes and "The Guardian's Duty" starts: go down to the
	//      Farm Village. The tree stays awake.
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
			m_Tree = Find(scene, "Sacred Tree");
			m_TreeAwake = Find(scene, "Sacred Tree Awake");
			m_TreeGlow = Find(scene, "Sacred Tree Glow");
			m_DialogueCanvas = Find(scene, "Dialogue Canvas");
			m_Cinematic = Find(scene, "Cinematic Canvas");
			m_LetterboxTop = Find(scene, "Letterbox Top");
			m_LetterboxBottom = Find(scene, "Letterbox Bottom");
			m_MarkerCanvas = Find(scene, "Quest Marker Canvas");
			m_Marker = Find(scene, "Quest Marker");
			if (m_TreeGlow.IsValid() && m_TreeGlow.HasComponent<SpriteRendererComponent>())
			{
				m_GlowEmission = m_TreeGlow.GetComponent<SpriteRendererComponent>().EmissionIntensity;
			}
			if (m_LetterboxTop.IsValid() && m_LetterboxTop.HasComponent<UIRectComponent>())
			{
				m_LetterboxHeight = m_LetterboxTop.GetComponent<UIRectComponent>().SizeDelta.y;
			}

			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag.rfind(SlimePrefix, 0) == 0 && scene.m_Registry.has<NativeScriptComponent>(entity))
				{
					m_Slimes.push_back(entity);
				}
			}

			UI::SetVisible(m_Cinematic, false);
			SetLetterbox(0.0f);
			QuestLog::Get().Claim(MonkName);
			// Coming back to the glade after the errand: the tree is awake and the monk only says goodbye.
			if (QuestLog::Get().Find(DutyQuest()) != nullptr)
			{
				m_Step = Step::Done;
				CreateTreeEffects(scene);
				SetTreeAwake(1.0f, 0.0f);
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
			if (m_DialogueCanvas.IsValid() == false || m_DialogueCanvas.HasComponent<NativeScriptComponent>() == false)
			{
				return nullptr;
			}
			for (NativeScriptComponent::Script& script : m_DialogueCanvas.GetComponent<NativeScriptComponent>().Scripts)
			{
				if (DialogueScript* dialogue = dynamic_cast<DialogueScript*>(script.Instance.get()))
				{
					return dialogue;
				}
			}
			return nullptr;
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
				for (NativeScriptComponent::Script& script : scene.m_Registry.get<NativeScriptComponent>(slime).Scripts)
				{
					if (EnemyChaseScript* enemy = dynamic_cast<EnemyChaseScript*>(script.Instance.get()))
					{
						enemy->Wake();
					}
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
			UI::SetVisible(m_Cinematic, true);
			// Where the camera looks: the tree's foot (an Upright sprite stands on its bottom edge), on its terrace.
			const glm::vec3 foot = TreeFoot(scene);
			DioramaView::CinematicFocus = glm::vec2(foot) + TreeFocusOffset;
			DioramaView::CinematicHeight = foot.z + TreeFocusRise;
			DioramaView::CinematicWeight = 0.0f;
			DioramaView::CinematicZoom = 1.0f;
			DioramaView::CinematicShake = 0.0f;
			CreateTreeEffects(scene);
			HY_INFO("Shrine quest: the tremor");
		}

		// The light the waking tree gives, and the motes rising round its trunk (both stay: the tree stays awake).
		void CreateTreeEffects(Scene& scene)
		{
			const glm::vec3 foot = TreeFoot(scene);
			if (m_TreeLight.IsValid() == false)
			{
				m_TreeLight = scene.CreateEntity("Light Sacred Tree");
				m_TreeLight.AddComponent<TransformComponent>().Translation = foot + glm::vec3(0.0f, -0.4f, 0.0f);
				Light2DComponent& light = m_TreeLight.AddComponent<Light2DComponent>();
				light.Type = Light2DType::Point;
				light.Color = GlowColor;
				light.Intensity = 0.0f;
				light.Radius = 7.5f;
				light.Height = 2.5f;
			}
			if (m_Motes.IsValid() == false)
			{
				m_Motes = scene.CreateEntity("Sacred Tree Motes");
				// View-space particles are placed up the standing tree, along the camera's up (DioramaView::Above).
				m_Motes.AddComponent<TransformComponent>().Translation = DioramaView::Above(foot + glm::vec3(0.0f, -0.6f, 0.0f), 0.8f);
				m_Motes.AddComponent<ParticleEmitter2DComponent>(Motes(0.0f));
			}
		}

		void UpdateTremor(Scene& scene, float deltaTime)
		{
			m_StepTime += deltaTime;
			const float t = m_StepTime;
			SetLetterbox(Smooth(t / 0.8f));
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
				// Leaves shaken loose from the canopy, and dust kicked up at its roots.
				m_LeavesFallen = true;
				const glm::vec3 foot = TreeFoot(scene);
				PlayEffect(scene, "Effect Falling Leaves", DioramaView::Above(foot, 7.5f), FallingLeaves());
				PlayEffect(scene, "Effect Root Dust", DioramaView::Above(foot + glm::vec3(0.0f, -0.6f, 0.0f), 0.2f), RootDust());
			}

			// The slow push in, and the tree waking from the tremor's start.
			DioramaView::CinematicZoom = 1.0f + (TreeZoom - 1.0f) * Smooth((t - 1.0f) / (TremorStart + AwakenTime));
			const float awake = Smooth(tremor / AwakenTime);
			SetTreeAwake(awake, m_Time);

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
			SetTreeAwake(1.0f, m_Time);
		}

		void UpdateRelease(float deltaTime)
		{
			m_StepTime += deltaTime;
			const float back = Smooth(m_StepTime / (GlideTime * 0.7f));
			DioramaView::CinematicWeight = 1.0f - back;
			DioramaView::CinematicZoom = TreeZoom + (1.0f - TreeZoom) * back;
			SetLetterbox(1.0f - Smooth(m_StepTime / 0.8f));
			if (back >= 1.0f)
			{
				DioramaView::ResetCinematic();
				SetLetterbox(0.0f);
				UI::SetVisible(m_Cinematic, false);
				QuestLog::Get().Complete(SlimesQuest());
				QuestLog::Get().Start(DutyQuest()); // take the road south to the Farm Village
				m_Step = Step::Done;
			}
		}

		// `amount` 0..1 of the waking (the overlay and the glow fade in, the light and motes grow); `time` drives
		// the breathing.
		void SetTreeAwake(float amount, float time)
		{
			const float breath = 0.5f + 0.5f * std::sin(time * 1.6f);
			if (m_TreeAwake.IsValid() && m_TreeAwake.HasComponent<SpriteRendererComponent>())
			{
				m_TreeAwake.GetComponent<SpriteRendererComponent>().Color.a = amount;
			}
			if (m_TreeGlow.IsValid() && m_TreeGlow.HasComponent<SpriteRendererComponent>())
			{
				SpriteRendererComponent& glow = m_TreeGlow.GetComponent<SpriteRendererComponent>();
				// The veins light up in pulses while it wakes, then settle into a slow breath.
				const float pulse = amount < 1.0f ? 0.6f + 0.4f * std::abs(std::sin(time * 3.2f)) : 0.75f + 0.25f * breath;
				glow.Color.a = std::min(1.0f, amount * 1.3f) * pulse;
				glow.EmissionIntensity = m_GlowEmission * (0.7f + 0.6f * pulse);
			}
			if (m_TreeLight.IsValid() && m_TreeLight.HasComponent<Light2DComponent>())
			{
				m_TreeLight.GetComponent<Light2DComponent>().Intensity = amount * (1.1f + 0.4f * breath);
			}
			if (m_Motes.IsValid() && m_Motes.HasComponent<ParticleEmitter2DComponent>())
			{
				m_Motes.GetComponent<ParticleEmitter2DComponent>().Rate = 2.0f + 10.0f * amount;
			}
		}

		void SetLetterbox(float amount)
		{
			const float offset = (1.0f - std::clamp(amount, 0.0f, 1.0f)) * m_LetterboxHeight;
			if (m_LetterboxTop.IsValid() && m_LetterboxTop.HasComponent<UIRectComponent>())
			{
				m_LetterboxTop.GetComponent<UIRectComponent>().AnchoredPosition.y = offset;
			}
			if (m_LetterboxBottom.IsValid() && m_LetterboxBottom.HasComponent<UIRectComponent>())
			{
				m_LetterboxBottom.GetComponent<UIRectComponent>().AnchoredPosition.y = -offset;
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
			const glm::vec3 foot = Foot(scene, m_Monk);
			const float bob = 0.08f * std::sin(m_Time * 3.0f);
			scene.SetWorldPosition(m_MarkerCanvas, DioramaView::Above(foot, 2.35f + bob));
			if (m_Marker.IsValid() && m_Marker.HasComponent<UIImageComponent>())
			{
				m_Marker.GetComponent<UIImageComponent>().Color = offer ? glm::vec4(1.0f, 0.42f, 0.3f, 1.0f) : glm::vec4(1.0f, 0.84f, 0.42f, 1.0f);
			}
		}

		// ---- Effects (plain component values, like the battle's) -------------------------------------------

		// The cyan motes rising round the trunk; Rate is set every frame (SetTreeAwake).
		ParticleEmitter2DComponent Motes(float rate) const
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = true;
			motes.Duration = 2.0f;
			motes.Rate = rate;
			motes.MaxParticles = 96;
			motes.Shape = EmitterShape::Box;
			motes.ShapeSize = { 3.6f, 0.8f };
			motes.Lifetime = { 2.5f, 4.5f };
			motes.Speed = { 0.4f, 1.1f };
			motes.Direction = 90.0f;
			motes.Spread = 14.0f;
			motes.StartSize = { 0.08f, 0.18f };
			motes.EndSizeScale = 0.3f;
			motes.Turbulence = 0.8f;
			motes.StartColor = glm::vec4(GlowColor, 0.9f);
			motes.EndColor = glm::vec4(GlowColor * 0.7f + glm::vec3(0.3f), 0.0f);
			motes.Space = ParticleSpace::View;
			motes.EmissionIntensity = 2.5f;
			motes.SortOrder = 200;
			return motes;
		}

		static ParticleEmitter2DComponent FallingLeaves()
		{
			ParticleEmitter2DComponent leaves;
			leaves.Looping = false;
			leaves.Duration = 2.5f;
			leaves.Rate = 22.0f;
			leaves.BurstCount = 12;
			leaves.MaxParticles = 96;
			leaves.Shape = EmitterShape::Box;
			leaves.ShapeSize = { 8.0f, 2.5f };
			leaves.Lifetime = { 2.0f, 3.5f };
			leaves.Speed = { 0.2f, 0.8f };
			leaves.Direction = -90.0f;
			leaves.Spread = 40.0f;
			leaves.StartSize = { 0.12f, 0.22f };
			leaves.EndSizeScale = 0.8f;
			leaves.StartRotation = { 0.0f, 360.0f };
			leaves.AngularVelocity = { -200.0f, 200.0f };
			leaves.Gravity = { 0.0f, -1.6f };
			leaves.Drag = 1.2f;
			leaves.Turbulence = 2.5f;
			leaves.StartColor = { 0.62f, 0.72f, 0.3f, 1.0f };
			leaves.EndColor = { 0.5f, 0.55f, 0.25f, 0.0f };
			leaves.Space = ParticleSpace::View;
			leaves.SortOrder = 190;
			leaves.DestroyWhenFinished = true;
			return leaves;
		}

		static ParticleEmitter2DComponent RootDust()
		{
			ParticleEmitter2DComponent dust;
			dust.Looping = false;
			dust.Duration = 1.5f;
			dust.Rate = 18.0f;
			dust.BurstCount = 16;
			dust.MaxParticles = 64;
			dust.Shape = EmitterShape::Box;
			dust.ShapeSize = { 5.0f, 0.6f };
			dust.Lifetime = { 0.8f, 1.6f };
			dust.Speed = { 0.3f, 1.0f };
			dust.Direction = 90.0f;
			dust.Spread = 70.0f;
			dust.StartSize = { 0.25f, 0.5f };
			dust.EndSizeScale = 2.0f;
			dust.Drag = 1.5f;
			dust.StartColor = { 0.66f, 0.58f, 0.46f, 0.5f };
			dust.EndColor = { 0.7f, 0.62f, 0.5f, 0.0f };
			dust.Space = ParticleSpace::View;
			dust.SortOrder = 180;
			dust.DestroyWhenFinished = true;
			return dust;
		}

		// A one-shot effect entity at `position` (a new entity is a root, so this is its place in the world).
		static void PlayEffect(Scene& scene, const char* name, const glm::vec3& position, const ParticleEmitter2DComponent& settings)
		{
			Entity effect = scene.CreateEntity(name);
			effect.AddComponent<TransformComponent>().Translation = position;
			effect.AddComponent<ParticleEmitter2DComponent>(settings);
		}

		// ---- Helpers -------------------------------------------------------------------------------------

		static float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		glm::vec3 TreeFoot(Scene& scene)
		{
			return Foot(scene, m_Tree);
		}

		// Where an Upright sprite stands: its entity sits at its middle, its feet half its height towards -Y.
		static glm::vec3 Foot(Scene& scene, Entity entity)
		{
			if (entity.IsValid() == false)
			{
				return glm::vec3(0.0f);
			}
			const glm::mat4 world = scene.GetWorldTransform(entity);
			const float height = glm::length(glm::vec3(world[1]));
			return glm::vec3(world[3]) - glm::vec3(0.0f, 0.5f * height, 0.0f);
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
		Entity m_Monk, m_Player, m_Tree, m_TreeAwake, m_TreeGlow, m_TreeLight, m_Motes;
		Entity m_DialogueCanvas, m_Cinematic, m_LetterboxTop, m_LetterboxBottom, m_MarkerCanvas, m_Marker;
		std::vector<entt::entity> m_Slimes;
		int m_SlimeCount = 0;
		float m_GlowEmission = 1.6f;
		float m_LetterboxHeight = 140.0f;
		bool m_LeavesFallen = false;
	};
}
