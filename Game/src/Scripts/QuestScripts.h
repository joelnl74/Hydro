#pragma once
#include "Hydro.h"
#include "BattleScripts.h"
#include "DialogueScripts.h"
#include "DioramaCamera.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// A simple quest system for the field scenes: prototype code, hardcoded on purpose (the definitions / data tables
// asset system replaces the quest data and the lines, and a story / cutscene system the Shrine Glade's director).
//   - QuestLog: the quests of a run (title, current objective, a count), the banners waiting to be shown and the
//     NPCs a quest speaks for. Shared by every field scene, like BattleState.
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
		struct Quest
		{
			std::string Id;
			std::string Title;
			std::string Objective;
			int Progress = 0;
			int Goal = 0;      // 0: the objective has no count
			bool Done = false;
		};

		struct Announcement
		{
			std::string Header; // "New Quest", "Quest Complete"
			std::string Title;
		};

		static inline std::vector<Quest> Quests;
		static inline std::vector<Announcement> Announcements; // oldest first; QuestTrackerScript shows and removes them
		static inline uint32_t Revision = 0;                   // bumped by every change (the tracker flashes)

		// A new run: no quests, no banners, no NPC spoken for.
		static void Reset()
		{
			Quests.clear();
			Announcements.clear();
			s_Claimed.clear();
			s_Talked.clear();
			Revision++;
		}

		static Quest* Find(const std::string& id)
		{
			for (Quest& quest : Quests)
			{
				if (quest.Id == id)
				{
					return &quest;
				}
			}
			return nullptr;
		}

		// Adds the quest (or restarts one with the same id) and announces it.
		static void Start(const std::string& id, const std::string& title, const std::string& objective, int goal = 0)
		{
			Quest* quest = Find(id);
			if (quest == nullptr)
			{
				Quests.push_back(Quest());
				quest = &Quests.back();
			}
			quest->Id = id;
			quest->Title = title;
			quest->Objective = objective;
			quest->Progress = 0;
			quest->Goal = goal;
			quest->Done = false;
			Announcements.push_back({ "New Quest", title });
			Revision++;
			HY_INFO("Quest: started '{}': {}", title, objective);
		}

		static void SetObjective(const std::string& id, const std::string& objective, int goal = 0)
		{
			if (Quest* quest = Find(id))
			{
				quest->Objective = objective;
				quest->Progress = 0;
				quest->Goal = goal;
				Revision++;
				HY_INFO("Quest: '{}': {}", quest->Title, objective);
			}
		}

		static void SetProgress(const std::string& id, int progress)
		{
			Quest* quest = Find(id);
			if (quest != nullptr && quest->Progress != progress)
			{
				quest->Progress = progress;
				Revision++;
				HY_INFO("Quest: '{}': {} / {}", quest->Title, progress, quest->Goal);
			}
		}

		static void Complete(const std::string& id)
		{
			Quest* quest = Find(id);
			if (quest != nullptr && quest->Done == false)
			{
				quest->Done = true;
				Announcements.push_back({ "Quest Complete", quest->Title });
				Revision++;
				HY_INFO("Quest: completed '{}'", quest->Title);
			}
		}

		static void Remove(const std::string& id)
		{
			Quests.erase(std::remove_if(Quests.begin(), Quests.end(), [&](const Quest& quest) { return quest.Id == id; }), Quests.end());
			Revision++;
		}

		// The quest the tracker shows: the newest that isn't done (nullptr when there is none).
		static const Quest* GetTracked()
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
		static void Claim(const std::string& npc)
		{
			if (std::find(s_Claimed.begin(), s_Claimed.end(), npc) == s_Claimed.end())
			{
				s_Claimed.push_back(npc);
			}
		}

		static void Release(const std::string& npc)
		{
			s_Claimed.erase(std::remove(s_Claimed.begin(), s_Claimed.end(), npc), s_Claimed.end());
		}

		static bool Talk(const std::string& npc)
		{
			if (std::find(s_Claimed.begin(), s_Claimed.end(), npc) == s_Claimed.end())
			{
				return false;
			}
			s_Talked = npc;
			return true;
		}

		// True once after the player talked to `npc` (and the quest claims it).
		static bool TakeTalk(const std::string& npc)
		{
			if (s_Talked != npc)
			{
				return false;
			}
			s_Talked.clear();
			return true;
		}

	private:
		static inline std::vector<std::string> s_Claimed;
		static inline std::string s_Talked;
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
			m_Revision = QuestLog::Revision;
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
			const QuestLog::Quest* quest = QuestLog::GetTracked();
			if (m_Revision != QuestLog::Revision)
			{
				m_Revision = QuestLog::Revision;
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

			UI::SetText(m_TrackerTitle, quest->Title);
			UI::SetText(m_TrackerObjective, quest->Objective);
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
				if (held || QuestLog::Announcements.empty())
				{
					return;
				}
				const QuestLog::Announcement next = QuestLog::Announcements.front();
				QuestLog::Announcements.erase(QuestLog::Announcements.begin());
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
	// The lines are in code until there is a dialogue asset; the timings and the camera are HY_PROPERTY fields,
	// tunable while playing.
	HY_SCRIPT()
	class ShrineQuestScript : public ScriptableEntity
	{
	public:
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
			// A new run starts with the first scene; a Shrine Glade entered from another scene keeps the log.
			if (SceneTransition::GetDepth() == 0)
			{
				QuestLog::Reset();
			}
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
			QuestLog::Claim(MonkName);
			// Coming back to the glade after the errand: the tree is awake and the monk only says goodbye.
			if (QuestLog::Find("ShrineDuty") != nullptr)
			{
				m_Step = Step::Done;
				CreateTreeEffects(scene);
				SetTreeAwake(1.0f, 0.0f);
			}
			else
			{
				QuestLog::Remove("ShrineSlimes");
				m_Step = Step::Offer;
			}
			HY_INFO("Shrine quest: talk to the Old Monk on the shrine terrace (E)");
		}

		void OnDestroy() override
		{
			QuestLog::Release(MonkName);
			DioramaView::ResetCinematic();
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			const bool talked = QuestLog::TakeTalk(MonkName);
			UpdateConversation(scene);

			switch (m_Step)
			{
				case Step::Offer:
					if (talked)
					{
						Say(GetIntroLines());
						m_Step = Step::Briefing;
					}
					break;
				case Step::Briefing:
					if (IsTalking() == false)
					{
						WakeSlimes(scene);
						QuestLog::Start("ShrineSlimes", "Clearing the Grounds", "Drive the slimes from the lower path", m_SlimeCount);
						m_Step = Step::Hunt;
					}
					break;
				case Step::Hunt:
				{
					const int left = CountSlimes(scene);
					QuestLog::SetProgress("ShrineSlimes", m_SlimeCount - left);
					if (talked)
					{
						Say(GetReminderLines(left));
					}
					if (left == 0 && IsTalking() == false)
					{
						QuestLog::SetObjective("ShrineSlimes", "Return to the Old Monk");
						m_Step = Step::Return;
					}
					break;
				}
				case Step::Return:
					if (talked)
					{
						Say(GetReportLines());
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
						Say(GetFarewellLines());
					}
					break;
			}

			UpdateMarker(scene);
		}

	private:
		enum class Step : uint8_t { Offer, Briefing, Hunt, Return, Report, Tremor, Awakened, Release, Done };

		struct Line
		{
			const char* Speaker;
			std::string Text;
		};

		// ---- The lines (in code until there is a dialogue asset) --------------------------------------------

		static std::vector<Line> GetIntroLines()
		{
			return {
				{ "Old Monk", "Ah, Miko. Good, you're here. Slimes have crept up from the lower path again. Three of them, by my count." },
				{ "Old Monk", "Leave them be and they'll be at the offerings by nightfall. A guardian keeps the grounds clean, so off you go." },
				{ "Miko", "Leave it to me, Master. They won't get past the lanterns." },
				{ "Old Monk", "Mind the steps on the way down. And come back and tell me when it's done." }
			};
		}

		static std::vector<Line> GetReminderLines(int left)
		{
			if (left <= 1)
			{
				return { { "Old Monk", "Just one left, I think. I can hear it squelching about down there." } };
			}
			return { { "Old Monk", "Still " + std::to_string(left) + " of them on the lower path, by the sound of it. Off you go." } };
		}

		static std::vector<Line> GetReportLines()
		{
			return {
				{ "Miko", "The lower path is clear, Master. Not a single slime left." },
				{ "Old Monk", "Well done. My old knees thank you. Now, before the evening prayers..." },
				{ "Old Monk", "...Hm? The wind has stopped. Miko, do you hear that? The bells are ringing, and there is no wind." }
			};
		}

		static std::vector<Line> GetAwakeLines()
		{
			return {
				{ "Old Monk", "The sacred tree... In all my years at this shrine, I have never once seen its roots wake." },
				{ "Old Monk", "That was no ordinary quake. It ran in a line, straight through the grounds, as if something beneath them was pulled tight." },
				{ "Miko", "Master... what does it mean?" },
				{ "Old Monk", "I don't know. The old tales say this shrine sits on a seal, one of many, and that the tree keeps watch over it." },
				{ "Old Monk", "The line runs south, toward the village. The old bridge, the well... if anything below us stirred, they will have felt it there too." },
				{ "Old Monk", "I must stay and mend the wards, or the next tremor may break them. So this falls to you." },
				{ "Old Monk", "Go down to the village, Miko. This is the duty of the shrine's guardian. Not a burden; the promise you made the day you took up the bell." },
				{ "Miko", "...I understand. I'll go, and I'll come back with answers." },
				{ "Old Monk", "Take the road south. And Miko, walk carefully. Old roads remember old things." }
			};
		}

		static std::vector<Line> GetFarewellLines()
		{
			return { { "Old Monk", "The road south leads to the village. Go on, I'll keep the lanterns lit for you." } };
		}

		// ---- Conversations: lines of several speakers, one dialogue box per run of the same speaker ----------

		void Say(const std::vector<Line>& lines)
		{
			m_Lines = lines;
			m_NextLine = 0;
		}

		// True while lines are waiting or the dialogue box is open.
		bool IsTalking()
		{
			DialogueScript* dialogue = FindDialogue();
			return m_NextLine < m_Lines.size() || (dialogue != nullptr && dialogue->IsOpen());
		}

		void UpdateConversation([[maybe_unused]] Scene& scene)
		{
			DialogueScript* dialogue = FindDialogue();
			if (m_NextLine >= m_Lines.size())
			{
				return;
			}
			if (dialogue == nullptr)
			{
				// A scene without the dialogue box: the lines go to the log.
				for (; m_NextLine < m_Lines.size(); m_NextLine++)
				{
					HY_INFO("{}: {}", m_Lines[m_NextLine].Speaker, m_Lines[m_NextLine].Text);
				}
				return;
			}
			if (dialogue->IsOpen())
			{
				return;
			}
			const std::string speaker = m_Lines[m_NextLine].Speaker;
			std::vector<std::string> run;
			while (m_NextLine < m_Lines.size() && speaker == m_Lines[m_NextLine].Speaker)
			{
				run.push_back(m_Lines[m_NextLine].Text);
				m_NextLine++;
			}
			dialogue->Open(speaker, run);
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
				Say(GetAwakeLines());
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
				QuestLog::Complete("ShrineSlimes");
				QuestLog::Start("ShrineDuty", "The Guardian's Duty", "Take the road south to the Farm Village");
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
		std::vector<Line> m_Lines;
		size_t m_NextLine = 0;
		Entity m_Monk, m_Player, m_Tree, m_TreeAwake, m_TreeGlow, m_TreeLight, m_Motes;
		Entity m_DialogueCanvas, m_Cinematic, m_LetterboxTop, m_LetterboxBottom, m_MarkerCanvas, m_Marker;
		std::vector<entt::entity> m_Slimes;
		int m_SlimeCount = 0;
		float m_GlowEmission = 1.6f;
		float m_LetterboxHeight = 140.0f;
		bool m_LeavesFallen = false;
	};
}
