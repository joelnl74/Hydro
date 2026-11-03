#pragma once

#include "Hydro.h"
#include "Data/BattleDefinitions.h"
#include "Data/DialogueDefinition.h"
#include "Data/QuestDefinition.h"
#include "Scripts/Common/DioramaCamera.h"
#include "State/BattleState.h"
#include "State/QuestLog.h"
#include "Views/DialogueScript.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Hydro
{
	// The countryside's part of "The Guardian's Duty": follow the leyline, help Orin fight at the ancient tree, then the rift
	// opens (docs/TestBed.md, "Countryside quest"). StartQuestWhenAlone starts the duty when the scene is played on its own.
	HY_SCRIPT()
	class CountrysideQuestScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultQuestTable = 5008924285669857658ull;     // Assets/Data/Quests.hytable
		static constexpr uint64_t DefaultDialogueTable = 5304418617221955049ull;  // Assets/Data/Dialogue/Leyline.hytable
		static constexpr uint64_t DefaultEncounterTable = 5637391413083366177ull; // Assets/Data/Encounters.hytable
		static constexpr int OrinHero = 1;                                         // BattleData::HeroIds

		HY_PROPERTY(Asset(DataTable, QuestDefinition), Tooltip("The quests: rows GuardiansDuty and AlongTheLeyline"))
		AssetHandle QuestTable = DefaultQuestTable;
		HY_PROPERTY(Asset(DataTable, DialogueDefinition), Tooltip("What Orin says: rows OrinMeet, OrinJoins, OrinRetry"))
		AssetHandle DialogueTable = DefaultDialogueTable;
		HY_PROPERTY(DataRow(EncounterDefinition), Tooltip("Who fights at the tree"))
		DataRowRef Encounter{ AssetHandle(DefaultEncounterTable), "LeyWisps" };
		HY_PROPERTY(Tooltip("The NPC this quest speaks for: the name after \"Npc \" in its tag"))
		std::string OrinName = "Orin";
		HY_PROPERTY(Range(1, 30), Tooltip("Metres from Orin at which he calls out"))
		float MeetRange = 7.0f;
		HY_PROPERTY(Color)
		glm::vec3 LeyColor{ 0.35f, 0.9f, 1.0f };
		HY_PROPERTY(Tooltip("Played on its own (not pushed from the Shrine Glade): start the duty at its leyline step"))
		bool StartQuestWhenAlone = true;
		HY_PROPERTY(Range(0, 1), Tooltip("How far the camera shakes (metres) while the ground opens"))
		float GateShake = 0.16f;
		HY_PROPERTY(Range(1, 8), Tooltip("Seconds from the first rumble until the rift has opened"))
		float GateOpenTime = 3.2f;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Player = Find(scene, "Player");
			m_Orin = Find(scene, "Npc " + OrinName);
			m_Tree = Find(scene, "Ley Tree");
			m_DialogueCanvas = Find(scene, "Dialogue Canvas");
			m_MarkerCanvas = Find(scene, "Ley Marker Canvas");
			m_Marker = Find(scene, "Ley Marker");
			m_Rift = Find(scene, "Ley Rift");
			m_RiftGlow = Find(scene, "Ley Rift Glow");
			m_GateArea = Find(scene, "Leyline Gate");
			SetRift(0.0f, 0.0f);
			auto view = scene.m_Registry.view<TagComponent>();
			for (int i = 0;; i++)
			{
				Entity marker = scene.FindEntityByTag("Leyline " + std::to_string(i));
				if (marker.IsValid() == false)
				{
					break;
				}
				m_Line.push_back(scene.GetWorldPosition(marker));
			}
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag.rfind("Ley Wisp", 0) == 0)
				{
					m_Wisps.push_back(Entity{ entity, &scene });
				}
			}
			QuestLog::Get().Claim(OrinName);

			BattleState& party = BattleState::Get();
			party.StartParty();
			if (StartQuestWhenAlone && SceneTransition::GetDepth() == 0 && QuestLog::Get().Find(DutyQuest()) == nullptr
				&& party.InParty(OrinHero) == false)
			{
				QuestLog::Get().Start(DutyQuest());
				QuestLog::Get().SetStage(DutyQuest(), 1);
				HY_INFO("Countryside quest: played on its own, so the duty starts at the leyline");
			}
			const QuestLog::Quest* duty = QuestLog::Get().Find(DutyQuest());
			if (party.InParty(OrinHero) || duty == nullptr || duty->Done)
			{
				// Orin travels with Miko already, or the shrine hasn't sent her yet: nothing happens here (an opened way stays open).
				RemoveEncounter(scene);
				m_Step = Step::Done;
				if (party.InParty(OrinHero) && QuestLog::Get().Find(LeylineQuest()) != nullptr)
				{
					CreateEffects(scene);
					OpenGate(scene, true);
				}
			}
			else
			{
				if (duty->Stage == 0)
				{
					QuestLog::Get().SetStage(DutyQuest(), 1); // "Follow the leyline across the rice fields"
				}
				m_Step = duty->Stage >= 2 ? Step::Retry : Step::Follow;
				CreateEffects(scene);
			}
			UI::SetVisible(m_MarkerCanvas, false);
		}

		void OnDestroy() override
		{
			QuestLog::Get().Release(OrinName);
			if (m_Gate == Gate::Opening || m_Gate == Gate::Speaking)
			{
				DioramaView::ResetCinematic();
			}
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			const bool talked = QuestLog::Get().TakeTalk(OrinName);
			UpdateConversation();

			switch (m_Step)
			{
				case Step::Follow:
					if (talked || Near(scene, m_Orin, MeetRange))
					{
						QuestLog::Get().SetStage(DutyQuest(), 2); // "Help the swordsman at the ancient tree"
						Say("OrinMeet");
						m_Step = Step::Meeting;
					}
					break;
				case Step::Retry:
					if (talked)
					{
						Say("OrinRetry");
						m_Step = Step::Meeting;
					}
					break;
				case Step::Meeting:
					if (IsTalking() == false)
					{
						StartBattle();
					}
					break;
				case Step::Battle:
					UpdateBattleResult(scene);
					break;
				case Step::Joining:
					if (IsTalking() == false)
					{
						QuestLog::Get().Complete(DutyQuest());
						QuestLog::Get().Start(LeylineQuest()); // "Find where the leyline sinks into the earth"
						RemoveEncounter(scene);
						m_Step = Step::Done;
						OpenGate(scene, false);
						HY_INFO("Countryside quest: Orin joins the party, and the ground opens");
					}
					break;
				case Step::Done:
					break;
			}

			UpdateGate(scene, deltaTime);
			PresentEffects(scene);
			PresentMarker(scene);
		}

	private:
		enum class Step : uint8_t { Follow, Meeting, Battle, Retry, Joining, Done };

		DataRowRef DutyQuest() const { return DataRowRef{ QuestTable, "GuardiansDuty" }; }
		DataRowRef LeylineQuest() const { return DataRowRef{ QuestTable, "AlongTheLeyline" }; }

		// ---- The way down into the leyline ----------------------------------------------------------------------

		enum class Gate : uint8_t { Sealed, Opening, Speaking, Open };

		// At once (the scene was entered with it open already), or with the rumble and the lines.
		void OpenGate(Scene& scene, bool atOnce)
		{
			if (m_Rift.IsValid() == false)
			{
				return;
			}
			const glm::vec3 rift = scene.GetWorldPosition(m_Rift);
			m_RiftLight = scene.CreateEntity("Light Ley Rift");
			m_RiftLight.AddComponent<TransformComponent>().Translation = rift;
			Light2DComponent& light = m_RiftLight.AddComponent<Light2DComponent>();
			light.Type = Light2DType::Point;
			light.Color = LeyColor;
			light.Intensity = 0.0f;
			light.Radius = 6.0f;
			light.Height = 0.6f;
			m_RiftMotes = scene.CreateEntity("Ley Rift Motes");
			m_RiftMotes.AddComponent<TransformComponent>().Translation = DioramaView::Above(rift, 0.2f);
			ParticleEmitter2DComponent motes = TreeMotes();
			motes.ShapeSize = { 2.2f, 0.6f };
			motes.Speed = { 0.6f, 1.6f };
			m_RiftMotes.AddComponent<ParticleEmitter2DComponent>(motes);
			m_GateTime = 0.0f;
			if (atOnce)
			{
				m_Gate = Gate::Open;
				SetRift(1.0f, 1.0f);
				return;
			}
			m_Gate = Gate::Opening;
			DioramaView::CinematicFocus = glm::vec2(rift);
			DioramaView::CinematicHeight = rift.z + 1.2f;
			DioramaView::CinematicWeight = 0.0f;
			DioramaView::CinematicShake = 0.0f;
		}

		void UpdateGate(Scene& scene, float deltaTime)
		{
			m_GateTime += deltaTime;
			const float t = m_GateTime;
			switch (m_Gate)
			{
				case Gate::Sealed:
					return;
				case Gate::Opening:
				{
					// The rumble stops dead as the rift opens.
					DioramaView::CinematicWeight = Smooth(t / 0.9f);
					const float open = Smooth((t - 0.45f * GateOpenTime) / (0.55f * GateOpenTime));
					const float surge = 0.75f + 0.25f * std::sin(t * 9.0f) * std::sin(t * 2.3f);
					DioramaView::CinematicShake = t < GateOpenTime ? GateShake * Smooth(t / 1.0f) * surge : 0.0f;
					SetRift(open, open * (1.0f + 1.5f * (1.0f - open)));
					SetGateEffects(open, 1.0f + 2.0f * open * (1.0f - open) * 4.0f);
					if (t >= GateOpenTime + 0.3f)
					{
						DioramaView::CinematicShake = 0.0f;
						Say("GateOpens");
						m_Gate = Gate::Speaking;
					}
					return;
				}
				case Gate::Speaking:
					SetGateEffects(1.0f, 1.0f);
					if (IsTalking() == false)
					{
						DioramaView::CinematicWeight = std::max(0.0f, DioramaView::CinematicWeight - deltaTime * 1.5f);
						if (DioramaView::CinematicWeight <= 0.0f)
						{
							m_Gate = Gate::Open;
							HY_INFO("Countryside quest: the way into the leyline is open (and sealed below)");
						}
					}
					return;
				case Gate::Open:
					break;
			}

			// Walking into the area says the way is sealed, once per visit to it.
			const float breath = 0.5f + 0.5f * std::sin(t * 1.4f);
			SetRift(1.0f, 0.75f + 0.25f * breath);
			SetGateEffects(1.0f, 0.8f + 0.4f * breath);
			const bool inside = InsideGate(scene);
			if (inside && m_GateArmed && IsTalking() == false && UI::CapturesInput(scene) == false)
			{
				m_GateArmed = false;
				Say("GateSealed");
				const QuestLog::Quest* quest = QuestLog::Get().Find(LeylineQuest());
				if (quest != nullptr && quest->Done == false && quest->Stage == 0)
				{
					QuestLog::Get().SetStage(LeylineQuest(), 1); // "The way into the leyline is not open yet"
				}
				HY_INFO("Countryside quest: the way into the leyline is sealed (for now)");
			}
			else if (inside == false)
			{
				m_GateArmed = true;
			}
		}

		// Opacity 0..1; the veins above 1 are brighter, through their emission.
		void SetRift(float rift, float glow)
		{
			if (m_Rift.IsValid() && m_Rift.HasComponent<SpriteRendererComponent>())
			{
				m_Rift.GetComponent<SpriteRendererComponent>().Color.a = std::clamp(rift, 0.0f, 1.0f);
			}
			if (m_RiftGlow.IsValid() && m_RiftGlow.HasComponent<SpriteRendererComponent>())
			{
				SpriteRendererComponent& veins = m_RiftGlow.GetComponent<SpriteRendererComponent>();
				veins.Color.a = std::clamp(glow, 0.0f, 1.0f);
				veins.EmissionIntensity = 2.5f * std::max(glow, 0.0f);
			}
		}

		void SetGateEffects(float open, float strength)
		{
			if (m_RiftLight.IsValid() && m_RiftLight.HasComponent<Light2DComponent>())
			{
				m_RiftLight.GetComponent<Light2DComponent>().Intensity = 1.6f * open * strength;
			}
			if (m_RiftMotes.IsValid() && m_RiftMotes.HasComponent<ParticleEmitter2DComponent>())
			{
				m_RiftMotes.GetComponent<ParticleEmitter2DComponent>().Rate = 14.0f * open * strength;
			}
		}

		// Its box collider's footprint, unscaled.
		bool InsideGate(Scene& scene) const
		{
			if (m_GateArea.IsValid() == false || m_Player.IsValid() == false)
			{
				return false;
			}
			glm::vec2 half{ 1.5f, 1.2f };
			glm::vec2 offset{ 0.0f };
			if (m_GateArea.HasComponent<BoxColliderComponent>())
			{
				const BoxColliderComponent& box = m_GateArea.GetComponent<BoxColliderComponent>();
				half = 0.5f * glm::vec2(box.Size);
				offset = glm::vec2(box.Offset);
			}
			const glm::vec2 d = glm::vec2(scene.GetWorldPosition(m_Player) - scene.GetWorldPosition(m_GateArea)) - offset;
			return std::abs(d.x) <= half.x && std::abs(d.y) <= half.y;
		}

		static float Smooth(float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		// ---- The battle at the tree ---------------------------------------------------------------------------

		void StartBattle()
		{
			const DataView<EncounterDefinition> encounter = Data::GetRow<EncounterDefinition>(Encounter);
			BattleState& party = BattleState::Get();
			if (encounter.IsValid() == false || party.BeginEncounter((uint64_t)GetEntity().GetUUID(), *encounter) == false)
			{
				HY_WARN("Countryside quest: can't start the battle at the tree (Encounter unset, or a battle is pending)");
				m_Step = Step::Retry;
				return;
			}
			party.Join(OrinHero); // he only stays if they win
			m_Step = Step::Battle;
			HY_INFO("Countryside quest: the battle at the ancient tree starts");
			SceneTransition::Push("Battle");
		}

		// The scene resumes under the popped battle: take its result once.
		void UpdateBattleResult([[maybe_unused]] Scene& scene)
		{
			BattleState& party = BattleState::Get();
			const BattleState::Result outcome = party.TakeOutcome((uint64_t)GetEntity().GetUUID());
			if (outcome == BattleState::Result::None)
			{
				return;
			}
			if (outcome == BattleState::Result::Won)
			{
				Say("OrinJoins");
				m_Step = Step::Joining;
				return;
			}
			party.Leave(OrinHero);
			m_Step = Step::Retry;
			HY_INFO("Countryside quest: the spirits are still there; talk to Orin to try again");
		}

		// Orin walks on with Miko and the spirits are gone: their entities go (no prompt, no collider left).
		void RemoveEncounter(Scene& scene)
		{
			if (m_Orin.IsValid())
			{
				scene.DestroyEntity(m_Orin);
				m_Orin = {};
			}
			for (Entity& wisp : m_Wisps)
			{
				if (wisp.IsValid())
				{
					scene.DestroyEntity(wisp);
				}
			}
			m_Wisps.clear();
		}

		// ---- Effects: the leyline's motes, the tree's glow, the circling spirits ------------------------------

		void CreateEffects(Scene& scene)
		{
			// A stream of motes per point, flowing towards the tree, and a faint light every third point.
			for (size_t i = 0; i < m_Line.size(); i++)
			{
				const glm::vec3 next = i + 1 < m_Line.size() ? m_Line[i + 1] : m_Line[i];
				const glm::vec2 way = glm::vec2(next - m_Line[i]);
				const float direction = glm::length(way) > 0.01f ? glm::degrees(std::atan2(way.y, way.x)) : 90.0f;
				Entity motes = scene.CreateEntity("Leyline Motes " + std::to_string(i));
				motes.AddComponent<TransformComponent>().Translation = m_Line[i] + glm::vec3(0.0f, 0.0f, 0.12f);
				motes.AddComponent<ParticleEmitter2DComponent>(LineMotes(direction));
				m_LineMotes.push_back(motes);
				if (i % 3 == 0)
				{
					Entity light = scene.CreateEntity("Light Leyline " + std::to_string(i));
					light.AddComponent<TransformComponent>().Translation = m_Line[i];
					Light2DComponent& glow = light.AddComponent<Light2DComponent>();
					glow.Type = Light2DType::Point;
					glow.Color = LeyColor;
					glow.Intensity = 0.0f;
					glow.Radius = 3.2f;
					glow.Height = 0.4f;
					m_LineLights.push_back(light);
				}
			}
			if (m_Tree.IsValid())
			{
				const glm::vec3 foot = scene.GetWorldPosition(m_Tree);
				m_TreeLight = scene.CreateEntity("Light Ley Tree");
				m_TreeLight.AddComponent<TransformComponent>().Translation = foot + glm::vec3(0.0f, -1.0f, 0.0f);
				Light2DComponent& light = m_TreeLight.AddComponent<Light2DComponent>();
				light.Type = Light2DType::Point;
				light.Color = LeyColor;
				light.Intensity = 0.0f;
				light.Radius = 8.0f;
				light.Height = 2.0f;
				m_TreeMotes = scene.CreateEntity("Ley Tree Motes");
				m_TreeMotes.AddComponent<TransformComponent>().Translation = DioramaView::Above(foot + glm::vec3(0.0f, -0.8f, 0.0f), 0.8f);
				m_TreeMotes.AddComponent<ParticleEmitter2DComponent>(TreeMotes());
			}
			for (Entity& wisp : m_Wisps)
			{
				m_WispHome.push_back(scene.GetWorldPosition(wisp));
			}
		}

		void PresentEffects(Scene& scene)
		{
			const bool active = m_Step != Step::Done || m_Gate != Gate::Sealed; // after the join the line runs into the rift
			for (size_t i = 0; i < m_LineMotes.size(); i++)
			{
				if (m_LineMotes[i].IsValid() && m_LineMotes[i].HasComponent<ParticleEmitter2DComponent>())
				{
					const float pulse = 0.5f + 0.5f * std::sin(m_Time * 2.4f - (float)i * 0.7f);
					m_LineMotes[i].GetComponent<ParticleEmitter2DComponent>().Rate = active ? 2.0f + 7.0f * pulse : 0.0f;
				}
			}
			for (size_t i = 0; i < m_LineLights.size(); i++)
			{
				if (m_LineLights[i].IsValid() && m_LineLights[i].HasComponent<Light2DComponent>())
				{
					const float pulse = 0.5f + 0.5f * std::sin(m_Time * 2.4f - (float)(i * 3) * 0.7f);
					m_LineLights[i].GetComponent<Light2DComponent>().Intensity = active ? 0.35f + 0.35f * pulse : 0.0f;
				}
			}
			const float breath = 0.5f + 0.5f * std::sin(m_Time * 1.6f);
			if (m_TreeLight.IsValid() && m_TreeLight.HasComponent<Light2DComponent>())
			{
				m_TreeLight.GetComponent<Light2DComponent>().Intensity = active ? 0.9f + 0.6f * breath : 0.5f + 0.2f * breath;
			}
			if (m_TreeMotes.IsValid() && m_TreeMotes.HasComponent<ParticleEmitter2DComponent>())
			{
				m_TreeMotes.GetComponent<ParticleEmitter2DComponent>().Rate = active ? 10.0f : 3.0f;
			}
			for (size_t i = 0; i < m_Wisps.size() && i < m_WispHome.size(); i++)
			{
				if (m_Wisps[i].IsValid())
				{
					const float angle = m_Time * 0.9f + (float)i * 2.1f;
					const glm::vec3 offset{ std::cos(angle) * 1.2f, std::sin(angle) * 0.6f, 0.15f * std::sin(m_Time * 2.3f + (float)i) };
					scene.SetWorldPosition(m_Wisps[i], m_WispHome[i] + offset);
				}
			}
		}

		ParticleEmitter2DComponent LineMotes(float direction) const
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = true;
			motes.Duration = 2.0f;
			motes.Rate = 0.0f;
			motes.MaxParticles = 48;
			motes.Shape = EmitterShape::Circle;
			motes.ShapeSize = { 0.7f, 0.7f };
			motes.Lifetime = { 1.6f, 2.8f };
			motes.Speed = { 0.9f, 1.6f };
			motes.Direction = direction;
			motes.Spread = 12.0f;
			motes.StartSize = { 0.07f, 0.15f };
			motes.EndSizeScale = 0.3f;
			motes.Turbulence = 0.4f;
			motes.StartColor = glm::vec4(LeyColor, 0.95f);
			motes.EndColor = glm::vec4(LeyColor * 0.7f + glm::vec3(0.3f), 0.0f);
			motes.Space = ParticleSpace::Ground;
			motes.EmissionIntensity = 2.5f;
			motes.SortOrder = 150;
			return motes;
		}

		ParticleEmitter2DComponent TreeMotes() const
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = true;
			motes.Duration = 2.0f;
			motes.Rate = 0.0f;
			motes.MaxParticles = 96;
			motes.Shape = EmitterShape::Box;
			motes.ShapeSize = { 4.0f, 0.8f };
			motes.Lifetime = { 2.5f, 4.5f };
			motes.Speed = { 0.4f, 1.1f };
			motes.Direction = 90.0f;
			motes.Spread = 14.0f;
			motes.StartSize = { 0.08f, 0.18f };
			motes.EndSizeScale = 0.3f;
			motes.Turbulence = 0.8f;
			motes.StartColor = glm::vec4(LeyColor, 0.9f);
			motes.EndColor = glm::vec4(LeyColor * 0.7f + glm::vec3(0.3f), 0.0f);
			motes.Space = ParticleSpace::View;
			motes.EmissionIntensity = 2.5f;
			motes.SortOrder = 200;
			return motes;
		}

		// Shown while he waits to be talked to after a lost battle.
		void PresentMarker(Scene& scene)
		{
			const bool shown = m_Step == Step::Retry && m_Orin.IsValid() && UI::CapturesInput(scene) == false;
			if (UI::IsVisible(m_MarkerCanvas) != shown)
			{
				UI::SetVisible(m_MarkerCanvas, shown);
			}
			if (shown && m_MarkerCanvas.IsValid())
			{
				const float bob = 0.08f * std::sin(m_Time * 3.0f);
				scene.SetWorldPosition(m_MarkerCanvas, DioramaView::Above(scene.GetWorldPosition(m_Orin), 2.35f + bob));
			}
		}

		// ---- Conversations ------------------------------------------------------------------------------------

		void Say(const char* row)
		{
			const DataView<DialogueDefinition> dialogue = Data::GetRow<DialogueDefinition>(DataRowRef{ DialogueTable, row });
			if (dialogue.IsValid() == false)
			{
				return; // warned once by Data::GetRow
			}
			m_Lines = DialogueScript::GetLines(*dialogue);
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
			if (dialogue->IsOpen() == false)
			{
				dialogue->Open(m_Lines);
				m_Waiting = false;
			}
		}

		DialogueScript* FindDialogue()
		{
			return m_DialogueCanvas.IsValid() ? m_DialogueCanvas.TryGetComponent<DialogueScript>() : nullptr;
		}

		// ---- Helpers ----------------------------------------------------------------------------------------

		bool Near(Scene& scene, const Entity& entity, float range) const
		{
			if (entity.IsValid() == false || m_Player.IsValid() == false)
			{
				return false;
			}
			const glm::vec2 offset = glm::vec2(scene.GetWorldPosition(entity) - scene.GetWorldPosition(m_Player));
			return glm::length(offset) < range;
		}

		static Entity Find(Scene& scene, const std::string& tag)
		{
			Entity entity = scene.FindEntityByTag(tag);
			if (entity.IsValid() == false)
			{
				HY_WARN("Countryside quest: the scene has no entity tagged \"{}\" (scripts/quests/build_countryside_quest.py adds it)", tag);
			}
			return entity;
		}

		Step m_Step = Step::Done;
		float m_Time = 0.0f;
		std::vector<DialogueScript::Line> m_Lines;
		bool m_Waiting = false;
		Entity m_Player, m_Orin, m_Tree, m_DialogueCanvas, m_MarkerCanvas, m_Marker, m_TreeLight, m_TreeMotes;
		Entity m_Rift, m_RiftGlow, m_GateArea, m_RiftLight, m_RiftMotes;
		Gate m_Gate = Gate::Sealed;
		float m_GateTime = 0.0f;
		bool m_GateArmed = true;
		std::vector<glm::vec3> m_Line;      // in order towards the tree
		std::vector<Entity> m_LineMotes, m_LineLights, m_Wisps;
		std::vector<glm::vec3> m_WispHome;
	};
}
