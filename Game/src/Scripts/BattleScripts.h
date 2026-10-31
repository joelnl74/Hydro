#pragma once
#include "Hydro.h"
#include "DioramaCamera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Native scripts of the Diorama RPG battle prototype (docs/DioramaRPG.md): enemies that wander and chase the
// player in the village (Assets/Scenes/StatueVillage.hyscene: slimes in the Slime Field, spirit wisps in the
// meadow and the field), and the party battle they start (Assets/Scenes/Battle.hyscene). The village is pushed
// down on the scene stack while the battle runs (SceneTransition) and carries on when it ends.
//
// The party is the shrine maiden Miko alone (HeroCount), with her sheet Characters/Miko (BattleIdle, Walk*, Punch,
// Kick, Death). The battle is a 3D forest arena (scripts/battle/build_arena.py builds Battle.hyscene); the code is
// written for any HeroCount, so a second hero is a row in GetHero, its entities in the scene and HeroCount + 1.
//
// Quick prototype code for play testing: the rules, the data and the presentation live together here. The HUD is
// runtime UI on the 1920 x 1080 canvas (scripts/ui/build_game_ui.py builds it): "Battle UI Canvas", a child of the
// Battle Director so its buttons' events reach this script, holds the turn order, the party's status, the message
// banner and the command and target lists; each enemy's shield and weaknesses are a World canvas ("Enemy <i> Info")
// pinned under its feet. The damage numbers, the target cursor and the turn marker are still sprites in the world.
//
// Battle keys: Up / Down (W / S) pick a command and Enter / Space / Z uses it (or click it). An attack or a
// damaging skill then opens the target list: Up / Down or Left / Right (A / D) move the cursor over the enemies,
// Enter (or a click) strikes, Esc / Backspace / X goes back to the commands. E / Q raise and lower the boost
// (spend Momentum: more hits or a stronger skill).
namespace Hydro
{
	// ---------------------------------------------------------------------------------------------------------
	// Data

	enum class DamageType : uint8_t { Slash = 0, Pierce, Blunt, Fire, Light, None };

	enum class EnemyKind : uint8_t { None = 0, Slime, Wisp };

	// Rows of Assets/Sprites/UI/BattleText.png (keep in step with scripts/shrine_glade/battle.py TEXT_ROWS and
	// NEW_TEXT, which appends the last three rows from the sheet's own glyphs).
	enum class BattleLabel : uint32_t
	{
		Attack = 0, FlameEdge, Crush, HolyLight, Heal, Guard, Potion, Run, Orin, Lila, Slime, Wisp,
		Weak, Resist, Break, Victory, Defeated, Escaped, Boost, NoMP, NoPotions, Critical, Guarding, NoEscape,
		Miko, FlameKick, HolyPalm
	};

	// The words a label stands for, as the runtime UI writes them (the command menu and the message box).
	inline const char* GetLabelText(BattleLabel label)
	{
		switch (label)
		{
		case BattleLabel::Attack: return "Attack";
		case BattleLabel::FlameEdge: return "Flame Edge";
		case BattleLabel::Crush: return "Crush";
		case BattleLabel::HolyLight: return "Holy Light";
		case BattleLabel::Heal: return "Heal";
		case BattleLabel::Guard: return "Guard";
		case BattleLabel::Potion: return "Potion";
		case BattleLabel::Run: return "Run";
		case BattleLabel::Orin: return "Orin";
		case BattleLabel::Lila: return "Lila";
		case BattleLabel::Slime: return "Slime";
		case BattleLabel::Wisp: return "Wisp";
		case BattleLabel::Weak: return "Weak!";
		case BattleLabel::Resist: return "Resist";
		case BattleLabel::Break: return "Break!";
		case BattleLabel::Victory: return "Victory!";
		case BattleLabel::Defeated: return "Defeated...";
		case BattleLabel::Escaped: return "Got away!";
		case BattleLabel::Boost: return "Boost";
		case BattleLabel::NoMP: return "Not enough MP";
		case BattleLabel::NoPotions: return "No potions left";
		case BattleLabel::Critical: return "Critical!";
		case BattleLabel::Guarding: return "Guarding";
		case BattleLabel::NoEscape: return "Couldn't get away!";
		case BattleLabel::Miko: return "Miko";
		case BattleLabel::FlameKick: return "Flame Kick";
		case BattleLabel::HolyPalm: return "Holy Palm";
		default: return "";
		}
	}

	// Cells of Assets/Sprites/UI/BattleIcons.png: row Icon, then row Digit (0-9 from DigitBase).
	struct BattleIcon
	{
		static constexpr uint32_t Unknown = 5;
		static constexpr uint32_t Shield = 6;
		static constexpr uint32_t PipEmpty = 7;
		static constexpr uint32_t PipFull = 8;
		static constexpr uint32_t Cursor = 9;
		static constexpr uint32_t Broken = 10;
		static constexpr uint32_t DigitBase = 12;
		static uint32_t ForType(DamageType type) { return (uint32_t)type; }
	};

	struct HeroDefinition
	{
		const char* Name;
		BattleLabel NameLabel;
		int MaxHP;
		int MaxMP;
		int Attack;
		int Magic;
		int Speed;
		DamageType WeaponType;
		// Skill A (key 2) and skill B (key 3). Heal targets the most hurt ally.
		BattleLabel SkillLabel[2];
		DamageType SkillType[2];
		int SkillCost[2];
		float SkillPower[2];
		bool SkillHeals[2];
		// The hero's clips (Characters/<Name>/<Name>.hyanim) for the two skills ("Punch" / "Kick"), whether a skill
		// walks up to its target first (a strike) or is used from where the hero stands, the item clip, and the
		// fighting-stance loop she holds between actions (and while guarding), not the field Idle.
		const char* SkillClip[2];
		bool SkillApproach[2];
		const char* ItemClip;
		const char* IdleClip;
	};

	// The party: Miko, the shrine maiden, fighting bare-handed (Blunt). Her Attack walks up to the target and
	// punches (a boost adds a kick, a punch, a kick); Flame Kick walks up and lands her spinning kick in fire;
	// Holy Palm walks up and strikes with her palm in light. The impacts land on
	// the clips' contact frames (BattleScript PunchImpactTime, KickImpactTime). Alone she carries the party's HP
	// and MP, so she is sturdier than Orin and Lila were on their own.
	constexpr int HeroCount = 1;

	inline const HeroDefinition& GetHero(int index)
	{
		static const HeroDefinition heroes[HeroCount] = {
			{ "Miko", BattleLabel::Miko, 90, 32, 10, 8, 9, DamageType::Blunt,
				{ BattleLabel::FlameKick, BattleLabel::HolyPalm }, { DamageType::Fire, DamageType::Light }, { 5, 6 }, { 1.6f, 1.7f }, { false, false }, { "Kick", "Punch" }, { true, true }, "BattleIdle", "BattleIdle" },
		};
		return heroes[index];
	}

	struct EnemyDefinition
	{
		const char* Name;
		BattleLabel NameLabel;
		int MaxHP;
		int Attack;
		int Speed;
		int Shield;
		std::array<DamageType, 3> Weak;   // DamageType::None = unused slot
		std::array<DamageType, 2> Resist;
	};

	inline const EnemyDefinition& GetEnemy(EnemyKind kind)
	{
		static const EnemyDefinition slime{ "Slime", BattleLabel::Slime, 30, 6, 4, 2,
			{ DamageType::Pierce, DamageType::Fire, DamageType::None }, { DamageType::Blunt, DamageType::None } };
		static const EnemyDefinition wisp{ "Spirit Wisp", BattleLabel::Wisp, 24, 7, 9, 3,
			{ DamageType::Fire, DamageType::Light, DamageType::None }, { DamageType::Slash, DamageType::Pierce } };
		return kind == EnemyKind::Wisp ? wisp : slime;
	}

	// What the village and the battle scene tell each other, and what the party keeps between battles.
	struct BattleState
	{
		enum class Result : uint8_t { None, Won, Lost, Fled };

		static constexpr int MaxEnemies = 3;
		static constexpr int MaxMomentum = 5;

		static inline int HeroHP[HeroCount] = { GetHero(0).MaxHP };
		static inline int HeroMP[HeroCount] = { GetHero(0).MaxMP };
		static inline bool Active = false;               // a battle was started and has not been settled yet
		static inline uint64_t EnemyId = 0;              // UUID of the village enemy that started it
		static inline Result Outcome = Result::None;     // set by the battle for that enemy to read when it is back
		static inline std::array<EnemyKind, MaxEnemies> Encounter{ EnemyKind::Slime, EnemyKind::None, EnemyKind::None };
		// Weaknesses the party has found, per enemy kind and weakness slot (kept for the whole game, as in a bestiary).
		static inline bool Revealed[3][3] = {};

		static void RestoreParty()
		{
			for (int i = 0; i < HeroCount; i++)
			{
				HeroHP[i] = GetHero(i).MaxHP;
				HeroMP[i] = GetHero(i).MaxMP;
			}
		}

		// A new game: forget what the previous run left behind.
		static void Reset()
		{
			RestoreParty();
			Active = false;
			EnemyId = 0;
			Outcome = Result::None;
			for (auto& kind : Revealed)
			{
				for (bool& slot : kind)
				{
					slot = false;
				}
			}
		}
	};

	// ---------------------------------------------------------------------------------------------------------
	// Village

	// An enemy for the village: wanders around where it was placed, runs after the entity tagged "Player" when
	// it comes close, gives up when the player gets far away, and starts a battle when it touches the player
	// (OnCollisionEnter: both need a body). Its tag decides what it is: tags starting with "Wisp" are spirit
	// wisps (faster, they lead a group of wisps and slimes), everything else is a slime. After a battle the
	// player did not win it goes home and ignores the player for a few seconds; after a win it disappears.
	// A Dormant one waits hidden until a quest wakes it (Wake(), the Shrine Glade's slimes, QuestScripts.h).
	// Put it on a Dynamic Rigidbody2D (GravityScale 0, high LinearDamping, FixedRotation) with a Box collider.
	HY_SCRIPT()
	class EnemyChaseScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 50), Tooltip("Metres: the player this close wakes it up"))
		float AggroRange = 5.5f;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres: a player without a physics body this close starts the battle"))
		float TouchRange = 0.7f;
		HY_PROPERTY(Range(0, 100), Tooltip("Metres: the player this far away makes it give up"))
		float LeashRange = 10.0f;
		HY_PROPERTY(Range(0, 100), Tooltip("Metres from home before it gives up the chase"))
		float MaxFromHome = 16.0f;
		HY_PROPERTY(Range(0, 50), Tooltip("Metres around home it wanders"))
		float WanderRadius = 3.0f;
		HY_PROPERTY(Range(0, 20), Tooltip("Metres per second while wandering (wisps move 15% faster)"))
		float WanderSpeed = 0.8f;
		HY_PROPERTY(Range(0, 20), Tooltip("Metres per second while chasing (the player walks at about 3.5)"))
		float ChaseSpeed = 2.2f;
		HY_PROPERTY(Range(0, 20), Tooltip("Metres per second walking back home"))
		float ReturnSpeed = 1.8f;
		HY_PROPERTY(Tooltip("Starts hidden, still and harmless, out of the 3D world, until Wake() (a quest sends it in)"))
		bool Dormant = false;

		// A dormant enemy appears where it was placed (fading in) and starts wandering. Nothing when it is awake.
		void Wake()
		{
			if (Dormant == false)
			{
				return;
			}
			Dormant = false;
			SetWorldPosition(m_DormantPlace);
			m_Appear = 0.0f;
			m_Wait = 0.6f;
			SetAlpha(0.0f);
		}

		bool IsDormant() const { return Dormant; }

	protected:
		void OnCreate() override
		{
			const glm::vec3 position = GetWorldPosition();
			m_Home = { position.x, position.y };
			m_Target = m_Home;
			m_Seed = 2463534242u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 7);
			m_Wait = 0.5f + 2.0f * Random01();
			if (GetEntity().HasComponent<TagComponent>() && GetComponent<TagComponent>().Tag.rfind("Wisp", 0) == 0)
			{
				m_Kind = EnemyKind::Wisp;
			}
			BattleState::Reset();

			// Dormant: parked far below the map, so its collider (kinematic, it has a script) blocks nobody, and
			// invisible.
			m_DormantPlace = position;
			if (Dormant)
			{
				SetWorldPosition(position - glm::vec3(0.0f, 0.0f, 100.0f));
				SetAlpha(0.0f);
			}
		}

		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();
			m_Time += deltaTime;

			if (Dormant)
			{
				Physics2D::SetLinearVelocity(self, glm::vec2(0.0f));
				return;
			}
			// Just woken: fades in over half a second, harmless meanwhile.
			if (m_Appear < 1.0f)
			{
				m_Appear = std::min(1.0f, m_Appear + deltaTime * 2.0f);
				SetAlpha(m_Appear);
				if (m_Appear < 1.0f)
				{
					return;
				}
			}

			// The battle this enemy started is over: the scene below carries on and reads the result.
			if (BattleState::Active && BattleState::EnemyId == (uint64_t)self.GetUUID() && BattleState::Outcome != BattleState::Result::None)
			{
				const BattleState::Result outcome = BattleState::Outcome;
				BattleState::Active = false;
				BattleState::Outcome = BattleState::Result::None;
				BattleState::EnemyId = 0;
				if (outcome == BattleState::Result::Won)
				{
					HY_INFO("{}: defeated, it is gone", GetEnemy(m_Kind).Name);
					scene->DestroyEntity(self);
					return;
				}

				m_Cooldown = 4.0f;
				m_State = State::Return;
			}

			const glm::vec3 world = GetWorldPosition();
			const glm::vec2 position{ world.x, world.y };
			m_Cooldown = std::max(0.0f, m_Cooldown - deltaTime);

			if (m_Player == entt::null || scene->m_Registry.valid(m_Player) == false)
			{
				m_Player = FindPlayer(*scene);
			}

			float playerDistance = 1.0e9f;
			if (m_Player != entt::null && scene->m_Registry.has<TransformComponent>(m_Player))
			{
				const glm::vec3 playerPosition = scene->GetWorldPosition(Entity{ m_Player, scene });
				m_PlayerPosition = { playerPosition.x, playerPosition.y };
				playerDistance = glm::length(m_PlayerPosition - position);

				// A player without a physics body (DioramaWalk in the Shrine Glade) never collides: touching
				// is being this close on the same level.
				if (scene->m_Registry.has<Rigidbody2DComponent>(m_Player) == false && playerDistance < TouchRange
					&& std::abs(playerPosition.z - world.z) < 0.5f)
				{
					StartBattle();
					return;
				}
			}

			const float homeDistance = glm::length(m_Home - position);
			const bool canChase = m_Cooldown <= 0.0f && BattleState::Active == false;
			if (canChase && m_State != State::Chase && playerDistance < AggroRange)
			{
				m_State = State::Chase;
				HY_INFO("{}: spotted the player", GetEnemy(m_Kind).Name);
			}
			else if (m_State == State::Chase && (canChase == false || playerDistance > LeashRange || homeDistance > MaxFromHome))
			{
				m_State = State::Return;
			}

			const float speedScale = m_Kind == EnemyKind::Wisp ? 1.15f : 1.0f;
			glm::vec2 velocity{ 0.0f };
			float animationSpeed = 1.0f;
			switch (m_State)
			{
				case State::Wander:
				{
					if (m_Wait > 0.0f)
					{
						m_Wait -= deltaTime;
						break;
					}

					m_WalkTime += deltaTime;
					const glm::vec2 toTarget = m_Target - position;
					if (glm::length(toTarget) < 0.25f || m_WalkTime > 4.0f)
					{
						// Rest, then pick a spot near home.
						m_WalkTime = 0.0f;
						m_Wait = 1.0f + 2.0f * Random01();
						const float angle = Random01() * 6.2831853f;
						const float radius = WanderRadius * std::sqrt(Random01());
						m_Target = m_Home + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
						break;
					}

					velocity = glm::normalize(toTarget) * WanderSpeed * speedScale;
					break;
				}
				case State::Chase:
				{
					const glm::vec2 toPlayer = m_PlayerPosition - position;
					if (glm::length(toPlayer) > 0.01f)
					{
						velocity = glm::normalize(toPlayer) * ChaseSpeed * speedScale;
					}
					animationSpeed = 1.8f;
					break;
				}
				case State::Return:
				{
					const glm::vec2 toHome = m_Home - position;
					if (glm::length(toHome) < 0.4f)
					{
						m_State = State::Wander;
						m_Wait = 1.0f;
						m_WalkTime = 0.0f;
						break;
					}

					velocity = glm::normalize(toHome) * ReturnSpeed * speedScale;
					break;
				}
			}

			// Wisps drift: a slow sideways sway on top of where they are going.
			if (m_Kind == EnemyKind::Wisp)
			{
				velocity.x += std::sin(m_Time * 1.7f + (float)(m_Seed & 7)) * 0.35f;
			}

			if (Physics2D::SetLinearVelocity(self, velocity) == false)
			{
				SetWorldPosition(world + glm::vec3(velocity * deltaTime, 0.0f));
			}

			SpriteAnimator::SetSpeed(self, animationSpeed);

			// A wisp's own light breathes a little.
			if (m_Kind == EnemyKind::Wisp && self.HasComponent<Light2DComponent>())
			{
				if (m_LightBase < 0.0f)
				{
					m_LightBase = GetComponent<Light2DComponent>().Intensity;
				}
				GetComponent<Light2DComponent>().Intensity = m_LightBase * (0.85f + 0.15f * std::sin(m_Time * 3.1f));
			}
		}

		void OnCollisionEnter(Entity other) override
		{
			if (other.HasComponent<TagComponent>() == false || other.GetComponent<TagComponent>().Tag != "Player")
			{
				return;
			}
			StartBattle();
		}

	private:
		// The village (or field) is paused under the battle scene. Once per touch: not while cooling down
		// after a battle or while one runs.
		void StartBattle()
		{
			if (m_Cooldown > 0.0f || BattleState::Active)
			{
				return;
			}

			// The village is paused under the battle: this enemy remembers it started it, and brings friends.
			Entity self = GetEntity();
			BattleState::Active = true;
			BattleState::EnemyId = (uint64_t)self.GetUUID();
			BattleState::Outcome = BattleState::Result::None;
			BattleState::Encounter = { m_Kind, EnemyKind::None, EnemyKind::None };
			const float roll = Random01();
			const int extra = roll < 0.3f ? 0 : (roll < 0.75f ? 1 : 2);
			for (int i = 1; i <= extra; i++)
			{
				BattleState::Encounter[i] = Random01() < (m_Kind == EnemyKind::Wisp ? 0.5f : 0.3f) ? EnemyKind::Wisp : EnemyKind::Slime;
			}

			Physics2D::SetLinearVelocity(self, glm::vec2(0.0f));
			HY_INFO("{}: touched the player, battle starts ({} enemies)", GetEnemy(m_Kind).Name, 1 + extra);
			SceneTransition::Push("Battle");
		}

		enum class State : uint8_t { Wander, Chase, Return };


		static entt::entity FindPlayer(Scene& scene)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == "Player")
				{
					return entity;
				}
			}

			return entt::null;
		}

		// 0 to 1 from a xorshift: the same enemies behave the same way every run, each differently.
		float Random01()
		{
			m_Seed ^= m_Seed << 13;
			m_Seed ^= m_Seed >> 17;
			m_Seed ^= m_Seed << 5;
			return (float)(m_Seed & 0xFFFFFF) / (float)0x1000000;
		}

		// The sprite's and its blob shadow's opacity (a dormant enemy is invisible, a woken one fades in).
		void SetAlpha(float alpha)
		{
			Entity self = GetEntity();
			if (self.HasComponent<SpriteRendererComponent>())
			{
				GetComponent<SpriteRendererComponent>().Color.a = alpha;
			}
			if (self.HasComponent<Shadow2DComponent>())
			{
				if (m_ShadowStrength < 0.0f)
				{
					m_ShadowStrength = GetComponent<Shadow2DComponent>().Strength;
				}
				GetComponent<Shadow2DComponent>().Strength = m_ShadowStrength * alpha;
			}
		}

		EnemyKind m_Kind = EnemyKind::Slime;
		State m_State = State::Wander;
		entt::entity m_Player = entt::null;
		glm::vec2 m_Home{ 0.0f };
		glm::vec2 m_Target{ 0.0f };
		glm::vec2 m_PlayerPosition{ 0.0f };
		float m_Wait = 1.0f;
		float m_WalkTime = 0.0f;
		float m_Cooldown = 0.0f; // seconds the enemy ignores the player (after a battle)
		float m_Time = 0.0f;
		float m_LightBase = -1.0f;
		float m_ShadowStrength = -1.0f;
		float m_Appear = 1.0f;          // 0..1 while a woken enemy fades in
		glm::vec3 m_DormantPlace{ 0.0f }; // where it was placed (a dormant one waits below it)
		uint32_t m_Seed = 1;
	};

	// ---------------------------------------------------------------------------------------------------------
	// Battle

	// The party battle: put it on one entity of Battle.hyscene (the "Battle Director"). The party against
	// up to three enemies (BattleState::Encounter). Rounds: everyone acts once, fastest first (the order is
	// shown at the top). On a hero's turn: 1 attack, 2 / 3 skills, 4 guard, 5 potion, 6 run, Left / Right pick
	// the target, Up / Down the boost.
	//
	// Rules (prototype values, tune freely):
	//   - Weak hits do 1.5x and take one point off the enemy's shield; resisted hits do 0.5x.
	//   - Shield at 0: the enemy is broken: it loses its turns this round and the next and takes 2x damage,
	//     then its shield comes back.
	//   - Momentum: each hero gains 1 per round (not in a round after boosting), up to 5. Boosting spends 1 to 3:
	//     an attack hits once more per point, a skill gets stronger.
	//   - Weaknesses show as "?" until hit with them, and stay known for the rest of the game.
	// It finds the scene's entities by tag (see scripts/shrine_glade/battle.py, which edits the scene).
	HY_SCRIPT()
	class BattleScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 99), Tooltip("Potions the party has in this battle (shown live while playing)"))
		int Potions = 3;

		// The stage camera (see "Stage camera" below). The scene's positions were baked with these values by
		// scripts/battle/build_arena.py (keep its CAMERA in step); changing them moves the view, the HUD stays put.
		HY_PROPERTY(Range(0, 89), Tooltip("Camera tilt in degrees from straight down (the field's is 50 to 70)"))
		float CameraTilt = 73.0f;
		HY_PROPERTY(Range(5, 90), Tooltip("Vertical field of view in degrees"))
		float CameraFieldOfView = 30.0f;
		HY_PROPERTY(Tooltip("The world point at the centre of the screen"))
		glm::vec3 CameraFocus{ 0.0f, 5.5f, 0.0f };
		HY_PROPERTY(Range(1, 100), Tooltip("Metres seen from the bottom of the screen to the top at the focus point"))
		float ViewHeight = 10.0f;

		// The hero's strikes: she walks up to the target, strikes on the clip's contact frame and walks back.
		HY_PROPERTY(Range(0.5, 30), Tooltip("Metres per second walking up to the target"))
		float WalkSpeed = 4.5f;
		HY_PROPERTY(Range(0.5, 30), Tooltip("Metres per second walking back"))
		float ReturnSpeed = 5.5f;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres right of the target where she stops to strike"))
		float StrikeDistance = 1.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Seconds into the Punch clip when the fist lands (Miko: its last frame, 0.5 s at 10 fps)"))
		float PunchImpactTime = 0.5f;
		HY_PROPERTY(Range(0, 2), Tooltip("Seconds into the Kick clip when the foot lands (Miko: frame 4, 0.4 s at 10 fps)"))
		float KickImpactTime = 0.4f;
		HY_PROPERTY(Range(0, 3), Tooltip("Length of the Punch clip in seconds; the next strike or the walk back starts after it"))
		float PunchLength = 0.65f;
		HY_PROPERTY(Range(0, 3), Tooltip("Length of the Kick clip in seconds"))
		float KickLength = 0.75f;
		HY_PROPERTY(Range(0, 3), Tooltip("Camera shake on impacts (0 = none)"))
		float ShakeStrength = 1.0f;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Camera = Find(scene, "Camera");
			UpdateStageCamera(scene);
			CaptureLayout(scene);
			for (int h = 0; h < HeroCount; h++)
			{
				const std::string prefix = std::string("Hero ") + GetHero(h).Name;
				Hero& hero = m_Heroes[h];
				hero.Sprite = Find(scene, prefix);
				hero.Shadow = Find(scene, prefix + " Shadow");
				// Her status on the right (runtime UI): "Status <Name>" ...
				const std::string status = std::string("Status ") + GetHero(h).Name;
				hero.StatusName = Find(scene, status + " Name");
				hero.StatusHP = Find(scene, status + " HP");
				hero.StatusMP = Find(scene, status + " MP");
				hero.StatusHPFill = Find(scene, status + " HP Fill");
				hero.StatusMPFill = Find(scene, status + " MP Fill");
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					hero.StatusPips[p] = Find(scene, status + " Pip " + std::to_string(p));
				}
				UI::SetText(EntityOf(hero.StatusName), GetHero(h).Name);
				hero.Base = ActorLayout(scene, hero.Sprite, HeroFoot, HeroPivot);
				hero.Size = ActorSize(scene, hero.Sprite);
				hero.Momentum = 1;
				if (BattleState::HeroHP[h] <= 0)
				{
					BattleState::HeroHP[h] = 1;
				}
			}

			int count = 0;
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				const std::string prefix = "Enemy " + std::to_string(e);
				Enemy& enemy = m_Enemies[e];
				enemy.Kind = BattleState::Encounter[e];
				enemy.Sprites[0] = Find(scene, prefix + " Slime");
				enemy.Sprites[1] = Find(scene, prefix + " Wisp");
				enemy.Shadows[0] = Find(scene, prefix + " Slime Shadow");
				enemy.Shadows[1] = Find(scene, prefix + " Wisp Shadow");
				enemy.Light = Find(scene, prefix + " Light");
				enemy.Info = Find(scene, prefix + " Info");
				enemy.HPFill = Find(scene, prefix + " HP Fill");
				enemy.ShieldIcon = Find(scene, prefix + " Shield");
				enemy.ShieldCount = Find(scene, prefix + " Shield Count");
				enemy.WeakLabel = Find(scene, prefix + " Weak Label");
				for (int w = 0; w < 3; w++)
				{
					enemy.WeakIcons[w] = Find(scene, prefix + " Weak " + std::to_string(w));
				}
				enemy.Base = ActorLayout(scene, enemy.Sprites[0], EnemyFoot, EnemyPivot[0]);
				enemy.Size[0] = ActorSize(scene, enemy.Sprites[0]);
				enemy.Size[1] = ActorSize(scene, enemy.Sprites[1]);
				if (enemy.Kind != EnemyKind::None)
				{
					const EnemyDefinition& definition = GetEnemy(enemy.Kind);
					enemy.HP = definition.MaxHP;
					enemy.Shield = definition.Shield;
					count++;
				}
			}

			for (int c = 0; c < 6; c++)
			{
				m_Commands[c] = Find(scene, "Command " + std::to_string(c));
				m_CommandNames[c] = Find(scene, "Command " + std::to_string(c) + " Name");
				m_CommandCosts[c] = Find(scene, "Command " + std::to_string(c) + " Cost");
				m_OrderChips[c] = Find(scene, "Order Chip " + std::to_string(c));
				m_OrderActors[c] = Find(scene, "Order Chip " + std::to_string(c) + " Actor");
			}
			for (int p = 0; p < 3; p++)
			{
				for (int d = 0; d < 3; d++)
				{
					m_Popups[p].Digits[d] = Find(scene, "Popup " + std::to_string(p) + " Digit " + std::to_string(d));
				}
			}
			m_Cursor = Find(scene, "Target Cursor");
			m_TurnMarker = Find(scene, "Turn Marker");
			m_CommandPanel = Find(scene, "Command Panel");
			m_CommandHeader = Find(scene, "Command Hero");
			m_CommandHelp = Find(scene, "Command Help");
			m_CommandBoost = Find(scene, "Command Boost");
			m_TargetPanel = Find(scene, "Target Panel");
			m_TargetHeader = Find(scene, "Target Command");
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				m_TargetButtons[e] = Find(scene, "Target " + std::to_string(e));
				m_TargetNames[e] = Find(scene, "Target " + std::to_string(e) + " Name");
				m_TargetHP[e] = Find(scene, "Target " + std::to_string(e) + " HP");
			}
			m_MessagePanel = Find(scene, "Battle Message Panel");
			m_MessageText = Find(scene, "Battle Message Text");
			// The menu opens on the hero's first turn (Present).
			UI::SetVisible(EntityOf(m_CommandPanel), false);
			UI::SetVisible(EntityOf(m_TargetPanel), false);
			UI::SetVisible(EntityOf(m_MessagePanel), false);
			m_Overlay = Find(scene, "Battle Overlay");
			m_StrikeEffect.Entity = Find(scene, "FX Strike");
			m_HealEffect.Entity = Find(scene, "FX Heal");

			// Keys held when the battle opens (the player was still walking) do not count.
			for (bool& previous : m_Previous)
			{
				previous = true;
			}

			HY_INFO("Battle: {} enemies! (Up/Down + Enter or click: command, then target; Esc back; E/Q boost)", count);
			SetPhase(Phase::Intro);
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			m_DeltaTime = deltaTime;
			m_ShakeTime = std::max(0.0f, m_ShakeTime - deltaTime);
			UpdateStageCamera(scene);
			m_PhaseTime += deltaTime;
			m_MessageTime = std::max(0.0f, m_MessageTime - deltaTime);
			for (Hero& hero : m_Heroes)
			{
				hero.Flash = std::max(0.0f, hero.Flash - deltaTime);
			}
			for (Enemy& enemy : m_Enemies)
			{
				enemy.Flash = std::max(0.0f, enemy.Flash - deltaTime);
			}
			for (Popup& popup : m_Popups)
			{
				popup.Time += deltaTime;
			}

			switch (m_Phase)
			{
				case Phase::Intro:
					if (m_PhaseTime >= 1.0f)
					{
						StartRound();
					}
					break;
				case Phase::NextTurn:
					NextTurn();
					break;
				case Phase::Command:
					UpdateCommand();
					break;
				case Phase::EnemyThink:
					if (m_PhaseTime >= 0.45f)
					{
						BeginEnemyAction();
					}
					break;
				case Phase::Acting:
					UpdateAction(scene);
					break;
				case Phase::Victory:
				case Phase::Defeat:
				case Phase::Fleeing:
					if (m_PhaseTime >= (m_Phase == Phase::Fleeing ? 0.9f : 2.0f))
					{
						Finish();
					}
					break;
				case Phase::Done:
					break;
			}

			Present(scene);
		}

		// The menus' buttons ("Command 0" to "Command 5", "Target 0" to "Target 2") bubble their events up to the
		// Battle Director: a command, the enemy under the cursor (focus), the target chosen, or Cancel (back).
		bool OnUIEvent(const UIEvent& event) override
		{
			const bool targeting = m_Phase == Phase::Command && m_Targeting;
			if (event.Type == UIEventType::Cancel)
			{
				if (targeting)
				{
					m_CancelTarget = true;
					return true;
				}
				return false;
			}
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				if ((entt::entity)event.Target != m_TargetButtons[e])
				{
					continue;
				}
				if (targeting && m_Enemies[e].Alive())
				{
					if (event.Type == UIEventType::FocusEnter)
					{
						m_Action.Target = e; // the cursor follows the focus (keys or the pointer)
					}
					else if (event.Type == UIEventType::Submit)
					{
						m_ChosenTarget = e;
					}
				}
				return true;
			}
			for (int c = 0; c < 6; c++)
			{
				if ((entt::entity)event.Target != m_Commands[c])
				{
					continue;
				}
				if (event.Type == UIEventType::FocusEnter)
				{
					m_HelpCommand = c; // the help line follows the focus
				}
				else if (event.Type == UIEventType::Submit && m_Phase == Phase::Command)
				{
					m_ChosenCommand = c;
				}
				return true;
			}
			return false;
		}

	private:
		enum class Phase : uint8_t { Intro, NextTurn, Command, EnemyThink, Acting, Victory, Defeat, Fleeing, Done };
		enum class Command : uint8_t { Attack = 0, SkillA, SkillB, Guard, Potion, Run, EnemyAttack };

		struct Hero
		{
			entt::entity Sprite = entt::null;
			entt::entity Shadow = entt::null; // "<name> Shadow": the contact shadow on the floor under her feet
			// Her status (runtime UI): name, HP and MP texts, the bars' fills (their width is the fraction), boost pips.
			entt::entity StatusName = entt::null, StatusHP = entt::null, StatusMP = entt::null;
			entt::entity StatusHPFill = entt::null, StatusMPFill = entt::null;
			entt::entity StatusPips[BattleState::MaxMomentum] = { entt::null, entt::null, entt::null, entt::null, entt::null };
			glm::vec2 Base{ 0.0f };   // where she stands, in layout units (see "Stage camera")
			glm::vec2 Offset{ 0.0f }; // from Base while she walks up to a target and back (layout units)
			float Size = 2.4f;        // the sprite's height in metres
			int Momentum = 1;
			bool BoostedLastRound = false;
			bool Guarding = false;
			float Flash = 0.0f;
		};

		struct Enemy
		{
			EnemyKind Kind = EnemyKind::None;
			entt::entity Sprites[2] = { entt::null, entt::null }; // slime, wisp
			entt::entity Shadows[2] = { entt::null, entt::null }; // their contact shadows ("<sprite> Shadow")
			entt::entity Light = entt::null;
			// Its info (a World canvas pinned under its feet): shield badge and count, the weakness label and icons, HP.
			entt::entity Info = entt::null, HPFill = entt::null, ShieldIcon = entt::null, ShieldCount = entt::null, WeakLabel = entt::null;
			entt::entity WeakIcons[3] = { entt::null, entt::null, entt::null };
			glm::vec2 Base{ 0.0f };
			float Size[2] = { 1.3f, 1.3f }; // the sprites' heights in metres (slime, wisp)
			int HP = 0;
			int Shield = 0;
			int BrokenUntilRound = -1; // broken while m_Round <= this
			float Flash = 0.0f;
			float Fade = 1.0f;         // 1 alive, falls to 0 when defeated
			bool Alive() const { return Kind != EnemyKind::None && HP > 0; }
		};

		struct Popup
		{
			entt::entity Digits[3] = { entt::null, entt::null, entt::null };
			glm::vec2 Position{ 0.0f };
			int Value = 0;
			glm::vec4 Color{ 1.0f };
			float Time = 10.0f;
		};

		// Actors: the heroes first (0 to HeroCount - 1), then the enemies.
		static constexpr int ActorCount = HeroCount + BattleState::MaxEnemies;
		static bool IsHero(int actor) { return actor < HeroCount; }
		static int EnemyIndex(int actor) { return actor - HeroCount; }

		struct Action
		{
			int Actor = 0;
			Command Kind = Command::Attack;
			int Target = 0;    // enemy index for attacks and damaging skills, hero index for heals and enemy attacks
			int Boost = 0;
			int HitsDone = 0;
			int Hits = 1;

			// A hero's strike sequence (UpdateHeroStrikes): walk up, strikes, walk back.
			enum class Stage : uint8_t { Approach, Strike, Return };
			Stage Step = Stage::Strike;
			float StepTime = 0.0f;
			float StepDuration = 0.0f;
			int Strike = 0;          // strikes started
			bool EffectStarted = false;
			bool Landed = false;
			glm::vec2 From{ 0.0f };  // the hero's offset at the start of a walk
			glm::vec2 To{ 0.0f };    // and at its end
		};

		// What a strike looks like: the hero's clip, and the effect that goes with it.
		enum class StrikeMove : uint8_t { Punch, Kick };

		// ---- Flow -----------------------------------------------------------------------------------------

		void SetPhase(Phase phase)
		{
			m_Phase = phase;
			m_PhaseTime = 0.0f;
			m_Targeting = false; // a new phase (or the next hero's turn) starts on the commands
		}

		void StartRound()
		{
			m_Round++;
			// Momentum, and shields come back once a break has run its course.
			for (int h = 0; h < HeroCount; h++)
			{
				Hero& hero = m_Heroes[h];
				if (hero.BoostedLastRound == false && m_Round > 1)
				{
					hero.Momentum = std::min(BattleState::MaxMomentum, hero.Momentum + 1);
				}
				hero.BoostedLastRound = false;
			}
			for (Enemy& enemy : m_Enemies)
			{
				if (enemy.Alive() && enemy.BrokenUntilRound >= 0 && m_Round > enemy.BrokenUntilRound)
				{
					enemy.BrokenUntilRound = -1;
					enemy.Shield = GetEnemy(enemy.Kind).Shield;
					HY_INFO("Battle: {} recovers its shield", GetEnemy(enemy.Kind).Name);
				}
			}

			// Fastest first, with a little luck.
			m_Order.clear();
			std::vector<std::pair<float, int>> speeds;
			for (int actor = 0; actor < ActorCount; actor++)
			{
				if (ActorAlive(actor))
				{
					const int speed = IsHero(actor) ? GetHero(actor).Speed : GetEnemy(m_Enemies[EnemyIndex(actor)].Kind).Speed;
					speeds.push_back({ (float)speed + 3.0f * Random01(), actor });
				}
			}
			std::sort(speeds.begin(), speeds.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
			for (const auto& entry : speeds)
			{
				m_Order.push_back(entry.second);
			}
			m_Turn = -1;
			HY_INFO("Battle: round {}", m_Round);
			SetPhase(Phase::NextTurn);
		}

		void NextTurn()
		{
			if (CheckEnd())
			{
				return;
			}

			m_Turn++;
			if (m_Turn >= (int)m_Order.size())
			{
				StartRound();
				return;
			}

			const int actor = m_Order[m_Turn];
			if (ActorAlive(actor) == false)
			{
				return; // next frame takes the next one
			}

			if (IsHero(actor))
			{
				m_Heroes[actor].Guarding = false;
				m_Action = Action{};
				m_Action.Actor = actor;
				m_Action.Target = FirstLivingEnemy(m_Target);
				m_Action.Boost = 0;
				SetPhase(Phase::Command);
				return;
			}

			Enemy& enemy = m_Enemies[EnemyIndex(actor)];
			if (IsBroken(enemy))
			{
				HY_INFO("Battle: {} is broken and cannot act", GetEnemy(enemy.Kind).Name);
				return;
			}

			m_Action = Action{};
			m_Action.Actor = actor;
			m_Action.Kind = Command::EnemyAttack;
			SetPhase(Phase::EnemyThink);
		}

		bool CheckEnd()
		{
			bool enemiesLeft = false;
			for (const Enemy& enemy : m_Enemies)
			{
				enemiesLeft = enemiesLeft || enemy.Alive();
			}
			if (enemiesLeft == false)
			{
				ShowMessage(BattleLabel::Victory);
				HY_INFO("Battle: victory!");
				SetPhase(Phase::Victory);
				return true;
			}

			bool heroesLeft = false;
			for (int h = 0; h < HeroCount; h++)
			{
				heroesLeft = heroesLeft || BattleState::HeroHP[h] > 0;
			}
			if (heroesLeft == false)
			{
				ShowMessage(BattleLabel::Defeated);
				HY_INFO("Battle: the party is defeated...");
				SetPhase(Phase::Defeat);
				return true;
			}

			return false;
		}

		void Finish()
		{
			if (m_Phase == Phase::Victory)
			{
				BattleState::Outcome = BattleState::Result::Won;
			}
			else if (m_Phase == Phase::Defeat)
			{
				// Down: the village sees the party back on their feet.
				BattleState::RestoreParty();
				BattleState::Outcome = BattleState::Result::Lost;
			}
			else
			{
				BattleState::Outcome = BattleState::Result::Fled;
			}

			// Fallen heroes get up with 1 HP after a won or fled battle.
			for (int h = 0; h < HeroCount; h++)
			{
				BattleState::HeroHP[h] = std::max(1, BattleState::HeroHP[h]);
			}
			SetPhase(Phase::Done);
			SceneTransition::Pop();
		}

		// ---- Hero input -----------------------------------------------------------------------------------

		void UpdateCommand()
		{
			const int h = m_Action.Actor;
			Hero& hero = m_Heroes[h];
			const HeroDefinition& definition = GetHero(h);

			// Up / Down move the menu's focus (the runtime UI), so the boost has keys of its own.
			if (Pressed(10, KeyCode::E))
			{
				m_Action.Boost = std::min({ 3, hero.Momentum, m_Action.Boost + 1 });
			}
			if (Pressed(12, KeyCode::Q))
			{
				m_Action.Boost = std::max(0, m_Action.Boost - 1);
			}

			// Choosing the target (the target list is open): Left / Right step the cursor along the enemies as they
			// stand (the list follows), Up / Down move it through the list (OnUIEvent), Enter strikes, Esc goes back.
			const int chosenTarget = m_ChosenTarget;
			const bool cancel = m_CancelTarget;
			m_ChosenTarget = -1;
			m_CancelTarget = false;
			// (read every frame, so a key held since the commands doesn't fire when the list opens)
			const bool leftArrow = Pressed(6, KeyCode::Left);
			const bool leftKey = Pressed(7, KeyCode::A);
			const bool rightArrow = Pressed(8, KeyCode::Right);
			const bool rightKey = Pressed(9, KeyCode::D);
			const bool left = leftArrow || leftKey;
			const bool right = rightArrow || rightKey;
			if (m_Targeting)
			{
				if (left || right)
				{
					m_Action.Target = StepTarget(m_Action.Target, left ? -1 : 1);
					UI::Focus(EntityOf(m_TargetButtons[m_Action.Target]));
				}
				if (cancel)
				{
					m_Targeting = false;
					m_ChosenCommand = -1;
					return;
				}
				if (chosenTarget >= 0 && m_Enemies[chosenTarget].Alive())
				{
					m_Targeting = false;
					m_Action.Target = chosenTarget;
					UseCommand(m_TargetCommand);
				}
				return;
			}

			// The command the menu submitted (OnUIEvent), once.
			const int chosen = m_ChosenCommand;
			m_ChosenCommand = -1;
			if (chosen < 0 || chosen >= 6)
			{
				return;
			}
			m_LastCommand[h] = chosen;
			const Command command = (Command)chosen;
			if (command == Command::SkillA || command == Command::SkillB)
			{
				const int s = command == Command::SkillA ? 0 : 1;
				if (BattleState::HeroMP[h] < definition.SkillCost[s])
				{
					ShowMessage(BattleLabel::NoMP);
					return;
				}
			}

			// An attack or a damaging skill picks its enemy first; the rest go straight ahead.
			const bool needsTarget = command == Command::Attack
				|| ((command == Command::SkillA || command == Command::SkillB) && definition.SkillHeals[command == Command::SkillA ? 0 : 1] == false);
			if (needsTarget)
			{
				m_Targeting = true;
				m_TargetCommand = chosen;
				m_Action.Target = std::max(0, FirstLivingEnemy(m_Action.Target));
				return;
			}
			UseCommand(chosen);
		}

		// Carries out command c for the hero whose turn it is, on m_Action.Target (an enemy for attacks and damaging
		// skills, chosen by the target list; the most hurt hero for heals and potions).
		void UseCommand(int c)
		{
			const int h = m_Action.Actor;
			Hero& hero = m_Heroes[h];
			const HeroDefinition& definition = GetHero(h);
			const Command command = (Command)c;
			if (command == Command::SkillA || command == Command::SkillB)
			{
				const int s = command == Command::SkillA ? 0 : 1;
				if (BattleState::HeroMP[h] < definition.SkillCost[s])
				{
					ShowMessage(BattleLabel::NoMP);
					return;
				}
				BattleState::HeroMP[h] -= definition.SkillCost[s];
				if (definition.SkillHeals[s])
				{
					m_Action.Target = MostHurtHero();
				}
			}
			else if (command == Command::Potion)
			{
				if (Potions <= 0)
				{
					ShowMessage(BattleLabel::NoPotions);
					return;
				}
				Potions--;
				m_Action.Target = MostHurtHero();
			}
			else if (command == Command::Run)
			{
				if (Random01() < 0.5f)
				{
					ShowMessage(BattleLabel::Escaped);
					HY_INFO("Battle: the party got away");
					SetPhase(Phase::Fleeing);
					return;
				}
				ShowMessage(BattleLabel::NoEscape);
				HY_INFO("Battle: could not escape");
				SetPhase(Phase::NextTurn);
				return;
			}

			m_Action.Kind = command;
			if (m_Action.Boost > 0)
			{
				hero.Momentum -= m_Action.Boost;
				hero.BoostedLastRound = true;
			}
			m_Action.Hits = command == Command::Attack ? 1 + m_Action.Boost : 1;
			m_Action.HitsDone = 0;
			m_Target = m_Action.Target;
			BeginHeroAction();
		}

		void BeginHeroAction()
		{
			const int h = m_Action.Actor;
			Entity sprite = EntityOf(m_Heroes[h].Sprite);
			const HeroDefinition& definition = GetHero(h);
			switch (m_Action.Kind)
			{
				case Command::Attack:
					BeginApproach();
					break;
				case Command::SkillA:
				case Command::SkillB:
				{
					const int s = m_Action.Kind == Command::SkillA ? 0 : 1;
					ShowMessage(definition.SkillLabel[s], 0.9f);
					if (definition.SkillHeals[s] == false && definition.SkillApproach[s])
					{
						BeginApproach();
					}
					else
					{
						BeginStrike();
					}
					break;
				}
				case Command::Potion:
					SpriteAnimator::Play(sprite, definition.ItemClip, true);
					ShowMessage(BattleLabel::Potion, 0.9f);
					break;
				case Command::Guard:
					m_Heroes[h].Guarding = true;
					SpriteAnimator::Play(sprite, definition.IdleClip, true);
					ShowMessage(BattleLabel::Guarding, 0.7f);
					break;
				default:
					break;
			}
			SetPhase(Phase::Acting);
		}

		// ---- The hero's strikes -----------------------------------------------------------------------------
		// Attack and the strike skills: walk up to the target (Walk<Facing>), one strike per hit on the clip's
		// contact frame (Punch / Kick), walk back, Idle. Spells (Holy Palm) strike from where she stands.

		StrikeMove CurrentMove() const
		{
			if (m_Action.Kind == Command::Attack)
			{
				return m_Action.Strike % 2 == 0 ? StrikeMove::Punch : StrikeMove::Kick; // punch, kick, punch, kick
			}
			const HeroDefinition& definition = GetHero(m_Action.Actor);
			return std::string(definition.SkillClip[m_Action.Kind == Command::SkillA ? 0 : 1]) == "Kick" ? StrikeMove::Kick : StrikeMove::Punch;
		}

		DamageType CurrentDamageType() const
		{
			const HeroDefinition& definition = GetHero(m_Action.Actor);
			if (m_Action.Kind == Command::Attack)
			{
				return definition.WeaponType;
			}
			return definition.SkillType[m_Action.Kind == Command::SkillA ? 0 : 1];
		}

		float ImpactTime(StrikeMove move) const { return move == StrikeMove::Punch ? PunchImpactTime : KickImpactTime; }
		float StrikeLength(StrikeMove move) const { return std::max(move == StrikeMove::Punch ? PunchLength : KickLength, ImpactTime(move) + 0.05f); }

		// Where the hero stands to strike the target: StrikeDistance to its right, on the floor.
		glm::vec2 StrikeOffset(int target) const
		{
			const glm::vec3 enemy = GroundPoint(m_Enemies[target].Base);
			return LayoutOf(enemy + glm::vec3(StrikeDistance, 0.0f, 0.0f)) - m_Heroes[m_Action.Actor].Base;
		}

		// Seconds to walk between two offsets of the hero at `speed` metres per second (measured on the floor).
		float WalkTime(const glm::vec2& from, const glm::vec2& to, float speed) const
		{
			const glm::vec2 base = m_Heroes[m_Action.Actor].Base;
			const float metres = glm::length(GroundPoint(base + to) - GroundPoint(base + from));
			return metres / std::max(speed, 0.1f);
		}

		void BeginApproach()
		{
			Hero& hero = m_Heroes[m_Action.Actor];
			m_Action.Step = Action::Stage::Approach;
			m_Action.StepTime = 0.0f;
			m_Action.From = hero.Offset;
			m_Action.To = StrikeOffset(m_Action.Target);
			m_Action.StepDuration = WalkTime(m_Action.From, m_Action.To, WalkSpeed);
			const glm::vec2 direction = m_Action.To - m_Action.From;
			SpriteAnimator::Play(EntityOf(hero.Sprite), direction.x <= 0.0f ? "WalkLeft" : "WalkRight", true);
			PlayEffect("Effect Dust", hero.Base + hero.Offset, 0.1f, StepDust(), ParticleSpace::Ground);
		}

		void BeginStrike()
		{
			m_Action.Step = Action::Stage::Strike;
			m_Action.StepTime = 0.0f;
			m_Action.EffectStarted = false;
			m_Action.Landed = false;
			SpriteAnimator::Play(EntityOf(m_Heroes[m_Action.Actor].Sprite), CurrentMove() == StrikeMove::Punch ? "Punch" : "Kick", true);
			m_Action.Strike++;
		}

		void BeginReturn()
		{
			Hero& hero = m_Heroes[m_Action.Actor];
			m_Action.Step = Action::Stage::Return;
			m_Action.StepTime = 0.0f;
			m_Action.From = hero.Offset;
			m_Action.To = glm::vec2(0.0f);
			m_Action.StepDuration = WalkTime(m_Action.From, m_Action.To, ReturnSpeed);
			if (m_Action.StepDuration > 0.01f)
			{
				SpriteAnimator::Play(EntityOf(hero.Sprite), m_Action.From.x > 0.0f ? "WalkLeft" : "WalkRight", true);
			}
		}

		void UpdateHeroStrikes()
		{
			Hero& hero = m_Heroes[m_Action.Actor];
			m_Action.StepTime += m_DeltaTime;
			const float k = m_Action.StepDuration > 0.0f ? std::clamp(m_Action.StepTime / m_Action.StepDuration, 0.0f, 1.0f) : 1.0f;
			switch (m_Action.Step)
			{
				case Action::Stage::Approach:
				{
					const float eased = 1.0f - (1.0f - k) * (1.0f - k); // brisk start, settles into place
					hero.Offset = m_Action.From + (m_Action.To - m_Action.From) * eased;
					if (k >= 1.0f)
					{
						PlayEffect("Effect Dust", hero.Base + hero.Offset, 0.1f, StepDust(), ParticleSpace::Ground);
						BeginStrike();
					}
					break;
				}
				case Action::Stage::Strike:
				{
					const StrikeMove move = CurrentMove();
					const float impact = ImpactTime(move);
					// The effect's sprite starts early enough that its burst frame meets the impact.
					if (m_Action.EffectStarted == false && m_Action.StepTime >= impact - EffectLead(move, CurrentDamageType()))
					{
						m_Action.EffectStarted = true;
						const int target = m_Enemies[m_Action.Target].Alive() ? m_Action.Target : FirstLivingEnemy(m_Action.Target);
						if (target >= 0 && IsHealing() == false)
						{
							PlayStrikeEffect(move, CurrentDamageType(), target);
						}
					}
					if (m_Action.Landed == false && m_Action.StepTime >= impact)
					{
						m_Action.Landed = true;
						LandStrike(move);
					}
					if (m_Action.StepTime >= StrikeLength(move))
					{
						const bool moreHits = m_Action.Kind == Command::Attack && m_Action.HitsDone < m_Action.Hits;
						const int next = FirstLivingEnemy(m_Action.Target);
						if (moreHits && next >= 0)
						{
							if (next != m_Action.Target)
							{
								// The target fell: walk over to the next one.
								m_Action.Target = next;
								BeginApproach();
							}
							else
							{
								BeginStrike();
							}
						}
						else
						{
							BeginReturn();
						}
					}
					break;
				}
				case Action::Stage::Return:
				{
					const float eased = k * k * (3.0f - 2.0f * k);
					hero.Offset = m_Action.From + (m_Action.To - m_Action.From) * eased;
					if (k >= 1.0f)
					{
						hero.Offset = glm::vec2(0.0f);
						EndAction();
					}
					break;
				}
			}
		}

		bool IsHealing() const
		{
			return (m_Action.Kind == Command::SkillA || m_Action.Kind == Command::SkillB)
				&& GetHero(m_Action.Actor).SkillHeals[m_Action.Kind == Command::SkillA ? 0 : 1];
		}

		// The strike lands: damage (or a heal), the impact's particles, light and shake.
		void LandStrike(StrikeMove move)
		{
			const HeroDefinition& definition = GetHero(m_Action.Actor);
			if (IsHealing())
			{
				const int s = m_Action.Kind == Command::SkillA ? 0 : 1;
				const float boost = 1.0f + 0.6f * (float)m_Action.Boost;
				HealHero(m_Action.Target, (int)std::lround(definition.SkillPower[s] * (float)definition.Magic * boost * (0.9f + 0.2f * Random01())));
				m_Action.HitsDone++;
				return;
			}

			if (m_Enemies[m_Action.Target].Alive() == false)
			{
				m_Action.Target = FirstLivingEnemy(m_Action.Target);
				if (m_Action.Target < 0)
				{
					return;
				}
			}

			bool strong = false;
			const DamageType type = CurrentDamageType();
			if (m_Action.Kind == Command::Attack)
			{
				strong = HitEnemy(m_Action.Target, type, (float)definition.Attack, 1.0f);
			}
			else
			{
				const int s = m_Action.Kind == Command::SkillA ? 0 : 1;
				const float boost = 1.0f + 0.6f * (float)m_Action.Boost;
				const float stat = type == DamageType::Light || type == DamageType::Fire ? (float)std::max(definition.Magic, definition.Attack) : (float)definition.Attack;
				strong = HitEnemy(m_Action.Target, type, stat, definition.SkillPower[s] * boost);
			}
			m_Action.HitsDone++;
			PlayImpact(move, type, m_Action.Target, strong);
		}

		void BeginEnemyAction()
		{
			// A random living hero.
			int living = 0;
			for (int h = 0; h < HeroCount; h++)
			{
				living += BattleState::HeroHP[h] > 0 ? 1 : 0;
			}
			int pick = std::min(living - 1, (int)(Random01() * (float)living));
			int target = 0;
			for (int h = 0; h < HeroCount; h++)
			{
				if (BattleState::HeroHP[h] > 0)
				{
					if (pick == 0)
					{
						target = h;
						break;
					}
					pick--;
				}
			}
			m_Action.Target = target;
			m_Action.Hits = 1;
			SetPhase(Phase::Acting);
		}

		// ---- Resolving --------------------------------------------------------------------------------------

		void UpdateAction(Scene& scene)
		{
			const bool hero = IsHero(m_Action.Actor);
			if (hero && (m_Action.Kind == Command::Attack || m_Action.Kind == Command::SkillA || m_Action.Kind == Command::SkillB))
			{
				UpdateHeroStrikes();
				return;
			}

			if (hero && m_Action.Kind == Command::Potion)
			{
				if (m_Action.HitsDone == 0 && m_PhaseTime >= ItemTime)
				{
					m_Action.HitsDone = 1;
					HealHero(m_Action.Target, 25);
				}
				if (m_PhaseTime >= 1.0f)
				{
					EndAction();
				}
				return;
			}

			if (hero)
			{
				// Guard.
				if (m_PhaseTime >= 0.6f)
				{
					EndAction();
				}
				return;
			}

			// An enemy lunges at its target.
			if (m_Action.HitsDone == 0 && m_PhaseTime >= 0.45f)
			{
				m_Action.HitsDone = 1;
				const Enemy& enemy = m_Enemies[EnemyIndex(m_Action.Actor)];
				int damage = (int)std::lround((float)GetEnemy(enemy.Kind).Attack * (0.85f + 0.4f * Random01()));
				Hero& target = m_Heroes[m_Action.Target];
				if (target.Guarding)
				{
					damage = std::max(1, damage / 2);
				}
				BattleState::HeroHP[m_Action.Target] = std::max(0, BattleState::HeroHP[m_Action.Target] - damage);
				target.Flash = 0.35f;
				SpawnPopup(ScreenAbove(target.Base, HeroPopupHeight), damage, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				PlayEffect("Effect Hit Sparks", target.Base, 0.9f, HitSparks(DamageType::Blunt, target.Guarding == false), ParticleSpace::View);
				Shake(target.Guarding ? 0.04f : 0.08f);
				Entity sprite = EntityOf(target.Sprite);
				if (BattleState::HeroHP[m_Action.Target] <= 0)
				{
					SpriteAnimator::Play(sprite, "Death", true);
					HY_INFO("Battle: {} falls!", GetHero(m_Action.Target).Name);
				}
				HY_INFO("Battle: {} hits {} for {}{}", GetEnemy(enemy.Kind).Name, GetHero(m_Action.Target).Name, damage, target.Guarding ? " (guarded)" : "");
			}
			if (m_PhaseTime >= 1.0f)
			{
				EndAction();
			}
			(void)scene;
		}

		void EndAction()
		{
			if (IsHero(m_Action.Actor))
			{
				Entity sprite = EntityOf(m_Heroes[m_Action.Actor].Sprite);
				m_Heroes[m_Action.Actor].Offset = glm::vec2(0.0f);
				if (BattleState::HeroHP[m_Action.Actor] > 0)
				{
					SpriteAnimator::Play(sprite, GetHero(m_Action.Actor).IdleClip);
				}
			}
			for (int h = 0; h < HeroCount; h++)
			{
				if (BattleState::HeroHP[h] > 0 && (h != m_Action.Actor || IsHero(m_Action.Actor) == false))
				{
					SpriteAnimator::Play(EntityOf(m_Heroes[h].Sprite), GetHero(h).IdleClip);
				}
			}
			SetPhase(Phase::NextTurn);
		}

		// Damage to an enemy, its popup and, when it falls, the defeat puff. Returns whether the hit was strong
		// (weak or critical): the impact effects (PlayImpact) are bigger then.
		bool HitEnemy(int index, DamageType type, float stat, float power)
		{
			Enemy& enemy = m_Enemies[index];
			const EnemyDefinition& definition = GetEnemy(enemy.Kind);
			float multiplier = 1.0f;
			bool weak = false;
			bool resisted = false;
			for (int w = 0; w < 3; w++)
			{
				if (definition.Weak[w] == type)
				{
					weak = true;
					multiplier = 1.5f;
					BattleState::Revealed[(int)enemy.Kind][w] = true;
				}
			}
			for (DamageType resist : definition.Resist)
			{
				if (resist == type)
				{
					resisted = true;
					multiplier = 0.5f;
				}
			}

			const bool critical = Random01() < 0.08f;
			if (critical)
			{
				multiplier *= 1.5f;
			}
			if (IsBroken(enemy))
			{
				multiplier *= 2.0f;
			}

			const int damage = std::max(1, (int)std::lround(stat * power * multiplier * (0.9f + 0.2f * Random01())));
			enemy.HP = std::max(0, enemy.HP - damage);
			enemy.Flash = 0.3f;

			glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
			if (weak && IsBroken(enemy) == false)
			{
				color = { 1.0f, 0.7f, 0.25f, 1.0f };
				enemy.Shield = std::max(0, enemy.Shield - 1);
				if (enemy.Shield == 0 && enemy.HP > 0)
				{
					enemy.BrokenUntilRound = m_Round + 1;
					ShowMessage(BattleLabel::Break);
					HY_INFO("Battle: {} is BROKEN!", definition.Name);
				}
				else
				{
					ShowMessage(BattleLabel::Weak, 0.6f);
				}
			}
			else if (resisted)
			{
				color = { 0.6f, 0.7f, 0.85f, 1.0f };
				ShowMessage(BattleLabel::Resist, 0.6f);
			}
			else if (critical)
			{
				ShowMessage(BattleLabel::Critical, 0.6f);
			}

			SpawnPopup(ScreenAbove(enemy.Base, EnemyPopupHeight), damage, color);
			HY_INFO("Battle: {} takes {}{}{}{} ({} HP left, shield {})", definition.Name, damage, weak ? " (weak)" : "", resisted ? " (resisted)" : "",
				critical ? " (critical)" : "", enemy.HP, enemy.Shield);
			if (enemy.HP <= 0)
			{
				HY_INFO("Battle: {} is defeated", definition.Name);
				PlayEffect("Effect Defeat Puff", enemy.Base, 0.5f, DefeatPuff(enemy.Kind), ParticleSpace::View);
			}
			return weak || critical;
		}

		// One-shot effect sprites (Battle.hyscene: "FX Strike", "FX Heal"): billboards in the world, placed,
		// restarted and shown while their clip runs; a point light on the same entity flares with them
		// (PresentSpriteEffect fades both), so the impact lights the floor and the actors around it (V9: a strong
		// effect alters the scene around it). They play alongside the particle effects below: the sprite gives the
		// authored shape, the particles the spray.
		struct SpriteEffect
		{
			entt::entity Entity = entt::null;
			float Time = 0.0f;     // seconds left
			float Duration = 1.0f;
			float LightPeak = 0.0f;  // the light's intensity at its flare
			float LightTime = 0.0f;  // seconds left of the flare
		};

		static constexpr float LightFlareTime = 0.35f;

		void PlaySpriteEffect(SpriteEffect& effect, const glm::vec3& position, const char* clip, float duration,
			const glm::vec3& lightColor = glm::vec3(1.0f), float lightIntensity = 0.0f)
		{
			Scene& scene = *GetEntity().GetScene();
			if (Alive(scene, effect.Entity) == false)
			{
				return;
			}
			scene.SetWorldPosition(EntityOf(effect.Entity), position);
			SpriteAnimator::Play(EntityOf(effect.Entity), clip, true);
			effect.Time = duration;
			effect.Duration = duration;
			effect.LightPeak = lightIntensity;
			effect.LightTime = lightIntensity > 0.0f ? LightFlareTime : 0.0f;
			if (scene.m_Registry.has<Light2DComponent>(effect.Entity))
			{
				scene.m_Registry.get<Light2DComponent>(effect.Entity).Color = lightColor;
			}
		}

		void PresentSpriteEffect(Scene& scene, SpriteEffect& effect)
		{
			if (Alive(scene, effect.Entity) == false)
			{
				return;
			}
			effect.Time = std::max(0.0f, effect.Time - m_DeltaTime);
			effect.LightTime = std::max(0.0f, effect.LightTime - m_DeltaTime);
			SetColor(scene, effect.Entity, glm::vec4(1.0f, 1.0f, 1.0f, effect.Time > 0.0f ? 1.0f : 0.0f));
			if (scene.m_Registry.has<Light2DComponent>(effect.Entity))
			{
				const float k = effect.LightTime / LightFlareTime;
				scene.m_Registry.get<Light2DComponent>(effect.Entity).Intensity = effect.LightPeak * k * k;
			}
		}

		// The clip on FX Strike (Battle/FX_Strikes.hyanim, scripts/battle/fx_strikes.py) for a strike: the punch's
		// starburst or the kick's swoosh, in fire for Fire (Flame Kick), as light for Light (Holy Palm).
		static const char* StrikeClip(StrikeMove move, DamageType type)
		{
			if (type == DamageType::Light)
			{
				return "Holy";
			}
			if (move == StrikeMove::Kick)
			{
				return type == DamageType::Fire ? "Flame" : "Kick";
			}
			return type == DamageType::Fire ? "Flame" : "Punch";
		}

		// Seconds the effect's sprite starts before the impact, so its burst frame meets it: the kick's swoosh
		// first follows the leg over the top (4 frames at 14 fps), the punch's burst is on its second frame.
		static float EffectLead(StrikeMove move, DamageType type)
		{
			const std::string clip = StrikeClip(move, type);
			if (clip == "Kick" || clip == "Flame")
			{
				return 4.0f / 14.0f;
			}
			return clip == "Holy" ? 1.0f / 12.0f : 1.0f / 14.0f;
		}

		// The effect's sprite at the target, lifted towards the camera so it is drawn over the enemy. The kick's
		// burst is left of its cell's centre (where the leg lands), so the cell sits a little to the right.
		void PlayStrikeEffect(StrikeMove move, DamageType type, int target)
		{
			const std::string clip = StrikeClip(move, type);
			const Enemy& enemy = m_Enemies[target];
			const float size = enemy.Size[enemy.Kind == EnemyKind::Wisp ? 1 : 0];
			glm::vec3 position = Above(enemy.Base, 0.55f * size) + m_Stage.Back * 0.6f;
			float duration = 6.0f / 14.0f;
			if (clip == "Kick" || clip == "Flame")
			{
				position += m_Stage.Right * 0.5f;
				duration = 7.0f / 14.0f;
			}
			else if (clip == "Holy")
			{
				position = Above(enemy.Base, 0.75f * size) + m_Stage.Back * 0.6f;
				duration = 8.0f / 12.0f;
			}
			PlaySpriteEffect(m_StrikeEffect, position, clip.c_str(), duration, StrikeLightColor(type, move), 0.0f);
		}

		static glm::vec3 StrikeLightColor(DamageType type, StrikeMove move)
		{
			switch (type)
			{
				case DamageType::Fire: return { 1.0f, 0.55f, 0.2f };
				case DamageType::Light: return { 1.0f, 0.93f, 0.7f };
				default: return move == StrikeMove::Kick ? glm::vec3(0.85f, 0.95f, 1.0f) : glm::vec3(1.0f, 0.85f, 0.55f);
			}
		}

		// The moment of impact: sparks in the effect's colours, the light flaring on the floor, a dust kick for a
		// kick, and the camera shake (stronger for weak and critical hits and for skills).
		void PlayImpact(StrikeMove move, DamageType type, int target, bool strong)
		{
			const Enemy& enemy = m_Enemies[target];
			const float size = enemy.Size[enemy.Kind == EnemyKind::Wisp ? 1 : 0];
			PlayEffect("Effect Hit Sparks", enemy.Base, 0.55f * size, HitSparks(type, strong), ParticleSpace::View);
			if (move == StrikeMove::Kick)
			{
				PlayEffect("Effect Dust", enemy.Base, 0.05f, KickDust(), ParticleSpace::Ground);
			}
			const bool skill = m_Action.Kind != Command::Attack;
			m_StrikeEffect.LightPeak = (skill ? 7.0f : 4.0f) * (strong ? 1.3f : 1.0f);
			m_StrikeEffect.LightTime = LightFlareTime;
			float shake = move == StrikeMove::Kick ? 0.09f : 0.06f;
			shake *= skill ? 1.8f : 1.0f;
			Shake(strong ? shake * 1.4f : shake);
		}

		void HealHero(int h, int amount)
		{
			if (BattleState::HeroHP[h] <= 0)
			{
				return;
			}
			const int before = BattleState::HeroHP[h];
			BattleState::HeroHP[h] = std::min(GetHero(h).MaxHP, BattleState::HeroHP[h] + amount);
			SpawnPopup(ScreenAbove(m_Heroes[h].Base, HeroPopupHeight), BattleState::HeroHP[h] - before, glm::vec4(0.45f, 1.0f, 0.55f, 1.0f));
			PlayEffect("Effect Heal", m_Heroes[h].Base, 0.35f, HealMotes(), ParticleSpace::View);
			PlaySpriteEffect(m_HealEffect, Above(m_Heroes[h].Base, 0.6f) + m_Stage.Back * 0.6f, "Heal", HealEffectTime,
				glm::vec3(0.55f, 1.0f, 0.6f), 2.5f);
			HY_INFO("Battle: {} recovers {} HP", GetHero(h).Name, BattleState::HeroHP[h] - before);
		}

		// ---- Helpers ----------------------------------------------------------------------------------------

		bool ActorAlive(int actor) const
		{
			return IsHero(actor) ? BattleState::HeroHP[actor] > 0 : m_Enemies[EnemyIndex(actor)].Alive();
		}

		bool IsBroken(const Enemy& enemy) const { return enemy.BrokenUntilRound >= m_Round && enemy.BrokenUntilRound >= 0; }

		int FirstLivingEnemy(int preferred) const
		{
			if (preferred >= 0 && preferred < BattleState::MaxEnemies && m_Enemies[preferred].Alive())
			{
				return preferred;
			}
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				if (m_Enemies[e].Alive())
				{
					return e;
				}
			}
			return -1;
		}

		int StepTarget(int current, int direction) const
		{
			for (int step = 1; step <= BattleState::MaxEnemies; step++)
			{
				const int e = (current + direction * step + BattleState::MaxEnemies * 2) % BattleState::MaxEnemies;
				if (m_Enemies[e].Alive())
				{
					return e;
				}
			}
			return current;
		}

		int MostHurtHero() const
		{
			int best = 0;
			float lowest = 2.0f;
			for (int h = 0; h < HeroCount; h++)
			{
				const float fraction = (float)BattleState::HeroHP[h] / (float)GetHero(h).MaxHP;
				if (BattleState::HeroHP[h] > 0 && fraction < lowest)
				{
					lowest = fraction;
					best = h;
				}
			}
			return best;
		}

		// The command menu (runtime UI): open with the focus on the hero's last command while she chooses, closed
		// otherwise; each row's name and cost, and the skills she can't pay for (or the potion when none are left)
		// greyed out (not Interactable, so neither the keys nor the pointer can pick them).
		void PresentCommandMenu(int hero, bool choosing)
		{
			const HeroDefinition& definition = GetHero(hero);
			const BattleLabel labels[6] = { BattleLabel::Attack, definition.SkillLabel[0], definition.SkillLabel[1], BattleLabel::Guard, BattleLabel::Potion, BattleLabel::Run };
			bool usable[6] = { true, true, true, true, true, true };
			for (int c = 0; c < 6; c++)
			{
				std::string cost;
				if (c == 1 || c == 2)
				{
					usable[c] = BattleState::HeroMP[hero] >= definition.SkillCost[c - 1];
					cost = std::to_string(definition.SkillCost[c - 1]) + " MP";
				}
				else if (c == 4)
				{
					usable[c] = Potions > 0;
					cost = "x" + std::to_string(Potions);
				}
				UI::SetText(EntityOf(m_CommandNames[c]), GetLabelText(labels[c]));
				UI::SetText(EntityOf(m_CommandCosts[c]), cost);
				UI::SetInteractable(EntityOf(m_Commands[c]), usable[c]);
			}
			UI::SetText(EntityOf(m_CommandHeader), GetLabelText(definition.NameLabel));
			UI::SetText(EntityOf(m_CommandBoost), m_Action.Boost > 0 ? "Boost " + std::to_string(m_Action.Boost) + "   (E / Q)" : std::string("E  Boost"));
			UI::SetText(EntityOf(m_CommandHelp), GetCommandHelp(hero, std::clamp(m_HelpCommand, 0, 5)));

			// The target list: one row per enemy still standing (the others are hidden, so the list closes up), with
			// a letter when two share a name, and their HP. Its header names the command.
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				const Enemy& enemy = m_Enemies[e];
				Entity row = EntityOf(m_TargetButtons[e]);
				if (UI::IsVisible(row) != enemy.Alive())
				{
					UI::SetVisible(row, enemy.Alive());
				}
				if (enemy.Alive() == false)
				{
					continue;
				}
				int same = 0;
				int before = 0;
				for (int other = 0; other < BattleState::MaxEnemies; other++)
				{
					if (m_Enemies[other].Kind == enemy.Kind && m_Enemies[other].Kind != EnemyKind::None)
					{
						same++;
						before += other < e ? 1 : 0;
					}
				}
				std::string name = GetLabelText(GetEnemy(enemy.Kind).NameLabel);
				if (same > 1)
				{
					name += std::string(" ") + (char)('A' + before);
				}
				UI::SetText(EntityOf(m_TargetNames[e]), name);
				UI::SetText(EntityOf(m_TargetHP[e]), "HP " + std::to_string(enemy.HP));
			}
			if (m_Targeting && m_TargetCommand >= 0 && m_TargetCommand < 6)
			{
				UI::SetText(EntityOf(m_TargetHeader), GetLabelText(labels[m_TargetCommand]));
			}

			// Which list is open: the commands, the targets (in the commands' place), or neither.
			const bool showCommands = choosing && m_Targeting == false;
			const bool showTargets = choosing && m_Targeting;
			if (showCommands != m_CommandsShown)
			{
				m_CommandsShown = showCommands;
				UI::SetVisible(EntityOf(m_CommandPanel), showCommands);
			}
			if (showTargets != m_TargetsShown)
			{
				m_TargetsShown = showTargets;
				UI::SetVisible(EntityOf(m_TargetPanel), showTargets);
			}
			// Opening a list (hiding the other drops its focus), or the focus was taken away meanwhile (another menu
			// closed over it): on the hero's last command, or on the enemy under the cursor.
			if (choosing && (bool)UI::GetFocused(*GetEntity().GetScene()) == false)
			{
				if (m_Targeting)
				{
					UI::Focus(EntityOf(m_TargetButtons[std::clamp(m_Action.Target, 0, BattleState::MaxEnemies - 1)]));
				}
				else
				{
					const int last = m_LastCommand[hero];
					UI::Focus(EntityOf(m_Commands[usable[last] ? last : 0]));
				}
			}
		}

		// The help line under the commands: what command c does for this hero.
		std::string GetCommandHelp(int hero, int c) const
		{
			const HeroDefinition& definition = GetHero(hero);
			switch ((Command)c)
			{
			case Command::Attack:
				return "Strike one foe with your bare hands. Boost: more blows.";
			case Command::SkillA:
			case Command::SkillB:
			{
				const int s = c == (int)Command::SkillA ? 0 : 1;
				if (definition.SkillHeals[s])
				{
					return "Restore the most hurt ally's HP.";
				}
				return std::string(GetDamageTypeName(definition.SkillType[s])) + " damage to one foe. Boost: stronger.";
			}
			case Command::Guard:
				return "Brace yourself: less damage until your next turn.";
			case Command::Potion:
				return "Restore 25 HP to the most hurt ally.";
			case Command::Run:
				return "Try to get away. Works half the time.";
			default:
				return "";
			}
		}

		static const char* GetDamageTypeName(DamageType type)
		{
			switch (type)
			{
			case DamageType::Slash: return "Slashing";
			case DamageType::Pierce: return "Piercing";
			case DamageType::Blunt: return "Blunt";
			case DamageType::Fire: return "Fire";
			case DamageType::Light: return "Light";
			default: return "No";
			}
		}

		void ShowMessage(BattleLabel label, float seconds = 1.2f)
		{
			m_MessageLabel = label;
			m_MessageTime = seconds;
		}

		void SpawnPopup(const glm::vec2& position, int value, const glm::vec4& color)
		{
			Popup& popup = m_Popups[m_NextPopup];
			m_NextPopup = (m_NextPopup + 1) % 3;
			popup.Position = position;
			popup.Value = std::clamp(value, 0, 999);
			popup.Color = color;
			popup.Time = 0.0f;
		}

		// ---- Effects (particles) ----------------------------------------------------------------------------
		// Each effect is a short-lived entity with a ParticleEmitter2DComponent: one burst, then it destroys
		// itself (DestroyWhenFinished). The presets are plain component values (Roadmap: prefabs later, V5).

		// Sparks where a hit lands, coloured by the damage type. Lit but glowing (emission, so they bloom), sprayed
		// in the plane facing the camera (PlayEffect makes battle effects View space). Sizes and speeds are in
		// layout units, like the rest of the battle: PlayEffect scales them to metres.
		static ParticleEmitter2DComponent HitSparks(DamageType type, bool strong)
		{
			ParticleEmitter2DComponent sparks;
			sparks.Looping = false;
			sparks.Duration = 0.05f;
			sparks.Rate = 0.0f;
			sparks.BurstCount = strong ? 30 : 18;
			sparks.MaxParticles = 48;
			sparks.Shape = EmitterShape::Circle;
			sparks.ShapeSize = { 0.25f, 0.25f };
			sparks.Lifetime = { 0.25f, 0.5f };
			sparks.Speed = { 4.0f, strong ? 10.0f : 7.5f };
			sparks.Direction = 90.0f;
			sparks.Spread = 180.0f;
			sparks.StartSize = { 0.12f, 0.28f };
			sparks.EndSizeScale = 0.2f;
			sparks.Gravity = { 0.0f, -9.0f };
			sparks.Drag = 3.0f;
			sparks.Lighting = ParticleLighting::Lit;
			sparks.EmissionIntensity = 3.0f;
			sparks.Blend = ParticleBlend::Additive; // drawn alpha until the additive pipeline (Roadmap V4)
			sparks.SortOrder = 200;
			sparks.DestroyWhenFinished = true;
			switch (type)
			{
				case DamageType::Fire:
					sparks.StartColor = { 1.0f, 0.75f, 0.3f, 1.0f };
					sparks.EndColor = { 1.0f, 0.25f, 0.05f, 0.0f };
					break;
				case DamageType::Light:
					sparks.StartColor = { 1.0f, 1.0f, 0.85f, 1.0f };
					sparks.EndColor = { 1.0f, 0.85f, 0.4f, 0.0f };
					break;
				default:
					sparks.StartColor = { 1.0f, 0.97f, 0.8f, 1.0f };
					sparks.EndColor = { 1.0f, 0.7f, 0.3f, 0.0f };
					break;
			}
			return sparks;
		}

		// A puff of smoke where a defeated enemy was. Lit: smoke is a thing in the scene, not a light.
		static ParticleEmitter2DComponent DefeatPuff(EnemyKind kind)
		{
			ParticleEmitter2DComponent puff;
			puff.Looping = false;
			puff.Duration = 0.05f;
			puff.Rate = 0.0f;
			puff.BurstCount = 26;
			puff.MaxParticles = 32;
			puff.Shape = EmitterShape::Circle;
			puff.ShapeSize = { 0.8f, 0.8f };
			puff.Lifetime = { 0.7f, 1.2f };
			puff.Speed = { 0.4f, 1.8f };
			puff.Direction = 90.0f;
			puff.Spread = 180.0f;
			puff.StartSize = { 0.6f, 1.1f };
			puff.EndSizeScale = 2.2f;
			puff.StartRotation = { 0.0f, 360.0f };
			puff.AngularVelocity = { -60.0f, 60.0f };
			puff.Gravity = { 0.0f, 0.9f };
			puff.Drag = 1.5f;
			puff.SortOrder = 25;
			puff.DestroyWhenFinished = true;
			if (kind == EnemyKind::Wisp)
			{
				puff.StartColor = { 0.75f, 0.95f, 1.0f, 0.8f };
				puff.EndColor = { 0.5f, 0.8f, 1.0f, 0.0f };
			}
			else
			{
				puff.StartColor = { 0.85f, 0.9f, 0.8f, 0.8f };
				puff.EndColor = { 0.6f, 0.65f, 0.6f, 0.0f };
			}
			return puff;
		}

		// Dust kicked up by a footstep (the hero setting off and stopping). On the floor (Ground space).
		static ParticleEmitter2DComponent StepDust()
		{
			ParticleEmitter2DComponent dust;
			dust.Looping = false;
			dust.Duration = 0.05f;
			dust.Rate = 0.0f;
			dust.BurstCount = 10;
			dust.MaxParticles = 16;
			dust.Shape = EmitterShape::Circle;
			dust.ShapeSize = { 0.4f, 0.4f };
			dust.Lifetime = { 0.35f, 0.6f };
			dust.Speed = { 0.6f, 1.6f };
			dust.Direction = 90.0f;
			dust.Spread = 180.0f;
			dust.StartSize = { 0.3f, 0.55f };
			dust.EndSizeScale = 1.8f;
			dust.Drag = 3.0f;
			dust.StartColor = { 0.62f, 0.52f, 0.42f, 0.55f };
			dust.EndColor = { 0.55f, 0.48f, 0.4f, 0.0f };
			dust.SortOrder = 20;
			dust.DestroyWhenFinished = true;
			return dust;
		}

		// A bigger burst of dust and grass bits where a kick lands.
		static ParticleEmitter2DComponent KickDust()
		{
			ParticleEmitter2DComponent dust = StepDust();
			dust.BurstCount = 18;
			dust.MaxParticles = 24;
			dust.Speed = { 1.5f, 3.5f };
			dust.Direction = 180.0f; // away from the hero, who strikes from the right
			dust.Spread = 70.0f;
			dust.StartSize = { 0.35f, 0.7f };
			return dust;
		}

		// Green motes rising from a healed hero.
		static ParticleEmitter2DComponent HealMotes()
		{
			ParticleEmitter2DComponent motes;
			motes.Looping = false;
			motes.Duration = 0.4f;
			motes.Rate = 40.0f;
			motes.BurstCount = 10;
			motes.MaxParticles = 40;
			motes.Shape = EmitterShape::Box;
			motes.ShapeSize = { 1.6f, 0.6f };
			motes.Lifetime = { 0.7f, 1.1f };
			motes.Speed = { 1.0f, 2.2f };
			motes.Direction = 90.0f;
			motes.Spread = 12.0f;
			motes.StartSize = { 0.15f, 0.3f };
			motes.EndSizeScale = 0.4f;
			motes.Drag = 0.8f;
			motes.Turbulence = 2.0f;
			motes.StartColor = { 0.55f, 1.0f, 0.6f, 1.0f };
			motes.EndColor = { 0.8f, 1.0f, 0.7f, 0.0f };
			motes.EmissionIntensity = 2.0f;
			motes.SortOrder = 200;
			motes.DestroyWhenFinished = true;
			return motes;
		}

		// Creates the effect entity `height` metres above `ground` (layout units), a little towards the camera.
		// The preset's lengths are layout units: scaled to metres here. Creating entities is allowed while scripts
		// run; the emitter starts in this frame's particle stage.
		void PlayEffect(const char* name, const glm::vec2& ground, float height, ParticleEmitter2DComponent settings, ParticleSpace space)
		{
			const float k = m_Stage.Scale;
			settings.Space = space;
			settings.ShapeSize *= k;
			settings.Speed.Min *= k;
			settings.Speed.Max *= k;
			settings.StartSize.Min *= k;
			settings.StartSize.Max *= k;
			settings.Gravity *= k;
			settings.Turbulence *= k;
			Entity effect = GetEntity().GetScene()->CreateEntity(name);
			// A new entity is a root, so its Translation is its place in the world.
			effect.AddComponent<TransformComponent>().Translation = Above(ground, height) + m_Stage.Back * 0.4f;
			effect.AddComponent<ParticleEmitter2DComponent>(settings);
		}

		bool Pressed(int index, KeyCode key)
		{
			const bool down = Input::IsKeyPressed(key);
			// While a runtime UI canvas captures the keyboard (a menu or dialogue, UI::CapturesInput) the battle's keys
			// don't count; they are still tracked, so a key held when it closes doesn't fire then.
			const bool pressed = down && m_Previous[index] == false && UI::CapturesInput(*GetEntity().GetScene()) == false;
			m_Previous[index] = down;
			return pressed;
		}

		float Random01() { return (float)(m_Random() & 0xFFFFFF) / (float)0x1000000; }

		Entity EntityOf(entt::entity entity) { return Entity{ entity, GetEntity().GetScene() }; }

		static entt::entity Find(Scene& scene, const std::string& tag)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == tag)
				{
					return entity;
				}
			}

			HY_WARN("Battle: the scene has no entity tagged \"{}\"", tag);
			return entt::null;
		}

		static bool Alive(Scene& scene, entt::entity entity)
		{
			return entity != entt::null && scene.m_Registry.valid(entity);
		}

		// ---- Stage camera -----------------------------------------------------------------------------------
		// The battle keeps its rules and its HUD in layout units (L): the 2D layout it had before the 3D stage,
		// LayoutHeight units from the bottom of the screen to the top, (0, 0) in the middle. The stage camera maps
		// them to the world every frame (scripts/battle/stage.py does the same for the builder):
		//   - HUD sprites (Overlay) sit on the plane facing the camera through the focus point (ScreenPoint), so
		//     they stay on screen whatever the camera does (shake, a changed tilt);
		//   - actors stand where the camera ray through their layout point meets the floor (GroundPoint), so they
		//     stand where the 2D battle had them on screen, on the 3D floor;
		//   - effects and popups go a height in metres above an actor (Above, ScreenAbove).
		static constexpr float LayoutHeight = 18.0f;
		static constexpr float HudLift = 1.0f;       // metres from the view plane towards the camera
		static constexpr float GroundHeight = 0.0f;  // the arena floor
		// Where the feet are in a cell, as a fraction of its height from the bottom, and the sheet's pivot (its
		// .hymeta): Characters/Player/Miko, Characters/Enemy (slime, wisp). An Upright sprite stands on its quad's
		// bottom edge, which a pivot moves back by Pivot x height, so StandOffset moves the sprite until the
		// feet are on the floor at its ground point (keep in step with scripts/battle/build_arena.py).
		static constexpr float HeroFoot = 15.0f / 96.0f;
		static constexpr float HeroPivot = 0.5f;
		static constexpr float EnemyFoot = 4.0f / 32.0f;
		static constexpr float EnemyPivot[2] = { 0.125f, 0.12f };
		// Metres above the floor: damage numbers, the turn marker and the target cursor.
		static constexpr float HeroPopupHeight = 1.7f;
		static constexpr float HeroMarkerHeight = 2.05f;
		static constexpr float EnemyPopupHeight = 1.35f;
		static constexpr float EnemyCursorHeight = 1.5f;

		struct StageCamera
		{
			glm::vec3 Focus{ 0.0f };
			glm::vec3 Right{ 1.0f, 0.0f, 0.0f };
			glm::vec3 Up{ 0.0f, 1.0f, 0.0f };
			glm::vec3 Forward{ 0.0f, 0.0f, -1.0f };
			glm::vec3 Back{ 0.0f, 0.0f, 1.0f };
			glm::vec3 Position{ 0.0f };
			float Distance = 1.0f;
			float Scale = 1.0f; // metres per layout unit at the focus point
		};

		// Places the camera from the properties (and the shake) and sets up the mapping for this frame.
		void UpdateStageCamera(Scene& scene)
		{
			const float tilt = glm::radians(std::clamp(CameraTilt, 0.0f, 89.0f));
			const float fieldOfView = std::clamp(CameraFieldOfView, 5.0f, 90.0f);
			StageCamera& stage = m_Stage;
			stage.Forward = { 0.0f, std::sin(tilt), -std::cos(tilt) };
			stage.Up = { 0.0f, std::cos(tilt), std::sin(tilt) };
			stage.Right = { 1.0f, 0.0f, 0.0f };
			stage.Back = -stage.Forward;
			stage.Distance = std::max(ViewHeight, 0.1f) / (2.0f * std::tan(glm::radians(fieldOfView) * 0.5f));
			stage.Scale = std::max(ViewHeight, 0.1f) / LayoutHeight;
			stage.Focus = CameraFocus;
			if (m_ShakeTime > 0.0f)
			{
				// A short, decaying jitter of the whole view (the HUD moves with the camera, so it stays still).
				const float amount = m_ShakeAmount * (m_ShakeTime / ShakeTime);
				stage.Focus += stage.Right * (std::sin(m_Time * 91.0f) * amount) + stage.Up * (std::cos(m_Time * 73.0f) * amount * 0.6f);
			}
			stage.Position = stage.Focus - stage.Forward * stage.Distance;

			if (Alive(scene, m_Camera) && scene.m_Registry.has<CameraComponent>(m_Camera))
			{
				DioramaView::PlaceInWorld(Entity{ m_Camera, &scene }, stage.Position, { tilt, 0.0f, 0.0f });
				SceneCamera& camera = scene.m_Registry.get<CameraComponent>(m_Camera).Camera;
				camera.Projection = ProjectionType::Perspective;
				camera.PerspectiveFOV = fieldOfView;
				camera.PerspectiveNear = 0.5f;
				camera.PerspectiveFar = stage.Distance + 200.0f;
				camera.OrthographicSize = ViewHeight;
			}
		}

		void Shake(float metres)
		{
			m_ShakeAmount = std::max(m_ShakeAmount * (m_ShakeTime / ShakeTime), metres * ShakeStrength);
			m_ShakeTime = ShakeTime;
		}

		glm::vec3 ScreenPoint(const glm::vec2& layout, float lift = HudLift) const
		{
			return m_Stage.Focus + (m_Stage.Right * layout.x + m_Stage.Up * layout.y) * m_Stage.Scale + m_Stage.Back * lift;
		}

		glm::vec3 GroundPoint(const glm::vec2& layout) const
		{
			const glm::vec3 onPlane = ScreenPoint(layout, 0.0f);
			const glm::vec3 direction = onPlane - m_Stage.Position;
			if (std::abs(direction.z) < 1.0e-5f)
			{
				return onPlane;
			}
			const float t = (GroundHeight - m_Stage.Position.z) / direction.z;
			return m_Stage.Position + direction * t;
		}

		// Any world point back to layout units: where it shows on screen.
		glm::vec2 LayoutOf(const glm::vec3& world) const
		{
			const glm::vec3 direction = world - m_Stage.Position;
			const float along = glm::dot(direction, m_Stage.Forward);
			if (std::abs(along) < 1.0e-5f)
			{
				return glm::vec2(0.0f);
			}
			const glm::vec3 offset = m_Stage.Position + direction * (m_Stage.Distance / along) - m_Stage.Focus;
			return glm::vec2(glm::dot(offset, m_Stage.Right), glm::dot(offset, m_Stage.Up)) / m_Stage.Scale;
		}

		// A point on (or lifted off) the view plane back to layout units.
		glm::vec2 PlaneLayoutOf(const glm::vec3& world) const
		{
			const glm::vec3 offset = world - m_Stage.Focus;
			return glm::vec2(glm::dot(offset, m_Stage.Right), glm::dot(offset, m_Stage.Up)) / m_Stage.Scale;
		}

		// `metres` above where an actor at `ground` (layout units) stands: up the standing sprite (Upright sprites
		// turn to face the camera, so their up is the camera's up).
		glm::vec3 Above(const glm::vec2& ground, float metres) const
		{
			return GroundPoint(ground) + m_Stage.Up * metres;
		}

		glm::vec2 ScreenAbove(const glm::vec2& ground, float metres) const
		{
			return LayoutOf(Above(ground, metres));
		}

		// From the point an Upright sprite's feet should stand on to its entity's position: back by the pivot,
		// then down the leaning sprite (the camera's up) by the feet's height in the cell. `size` in metres.
		glm::vec3 StandOffset(float foot, float pivot, float size) const
		{
			return glm::vec3(0.0f, pivot * size, 0.0f) - m_Stage.Up * (foot * size);
		}

		// Where an actor stands (its feet on the floor), in layout units; and its sprite's height in metres.
		glm::vec2 ActorLayout(Scene& scene, entt::entity entity, float foot, float pivot) const
		{
			if (Alive(scene, entity) == false)
			{
				return glm::vec2(0.0f);
			}
			const TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
			const glm::vec3 feet = scene.GetWorldPosition(Entity{ entity, &scene }) - StandOffset(foot, pivot, transform.Scale.y);
			return LayoutOf(glm::vec3(feet.x, feet.y, GroundHeight));
		}

		static float ActorSize(Scene& scene, entt::entity entity)
		{
			return Alive(scene, entity) ? scene.m_Registry.get<TransformComponent>(entity).Scale.y : 1.0f;
		}

		// An actor's sprite with its feet at `ground` (layout units), `lift` metres up (a wisp floats); its contact
		// shadow (an entity of its own, so it stays on the floor) under the feet.
		void PlaceActor(Scene& scene, entt::entity entity, entt::entity shadow, const glm::vec2& ground, float lift, float foot, float pivot)
		{
			const glm::vec3 feet = GroundPoint(ground);
			if (Alive(scene, entity))
			{
				const float size = scene.m_Registry.get<TransformComponent>(entity).Scale.y;
				scene.SetWorldPosition(Entity{ entity, &scene }, feet + StandOffset(foot, pivot, size) + glm::vec3(0.0f, 0.0f, lift));
			}
			if (Alive(scene, shadow))
			{
				scene.SetWorldPosition(Entity{ shadow, &scene }, feet);
			}
		}

		// Every HUD sprite (Overlay) with its place and size in layout units, read from where the builder put it.
		struct LayoutItem
		{
			glm::vec2 Position{ 0.0f };
			glm::vec2 Size{ 1.0f };
		};

		void CaptureLayout(Scene& scene)
		{
			m_Layout.clear();
			auto view = scene.m_Registry.view<TransformComponent, SpriteRendererComponent>();
			for (auto entity : view)
			{
				if (view.get<SpriteRendererComponent>(entity).Overlay == false)
				{
					continue;
				}
				const TransformComponent& transform = view.get<TransformComponent>(entity);
				LayoutItem item;
				item.Position = PlaneLayoutOf(scene.GetWorldPosition(Entity{ entity, &scene }));
				item.Size = glm::vec2(transform.Scale) / m_Stage.Scale;
				m_Layout[entity] = item;
			}
		}

		LayoutItem LayoutItemOf(entt::entity entity) const
		{
			const auto found = m_Layout.find(entity);
			return found != m_Layout.end() ? found->second : LayoutItem{};
		}

		// Puts every HUD sprite at its layout place for this frame's camera (last in Present).
		void ApplyLayout(Scene& scene)
		{
			for (const auto& [entity, item] : m_Layout)
			{
				if (Alive(scene, entity) == false)
				{
					continue;
				}
				scene.SetWorldPosition(Entity{ entity, &scene }, ScreenPoint(item.Position));
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
				transform.Scale.x = item.Size.x * m_Stage.Scale;
				transform.Scale.y = item.Size.y * m_Stage.Scale;
			}
		}

		static void SetColor(Scene& scene, entt::entity entity, const glm::vec4& color)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).Color = color;
			}
		}

		static void SetSprite(Scene& scene, entt::entity entity, uint32_t index)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).SpriteIndex = index;
			}
		}

		// Moves a HUD sprite to `position` (layout units); ApplyLayout puts it in the world.
		void SetPosition([[maybe_unused]] Scene& scene, entt::entity entity, const glm::vec2& position)
		{
			const auto found = m_Layout.find(entity);
			if (found != m_Layout.end())
			{
				found->second.Position = position;
			}
		}

		// A UI bar's fill: anchored to the bar's left edge, as wide as the fraction (UI rect anchors), in `color`.
		static void SetFill(Entity fill, float fraction, const glm::vec4& color)
		{
			if (fill.IsValid() == false || fill.HasComponent<UIRectComponent>() == false)
			{
				return;
			}
			UIRectComponent& rect = fill.GetComponent<UIRectComponent>();
			rect.AnchorMin = { 0.0f, 0.0f };
			rect.AnchorMax = { std::clamp(fraction, 0.0f, 1.0f), 1.0f };
			rect.SizeDelta = { 0.0f, 0.0f };
			rect.AnchoredPosition = { 0.0f, 0.0f };
			SetUIColor(fill, color);
		}

		// A UI element's image and text colour.
		static void SetUIColor(Entity element, const glm::vec4& color)
		{
			if (element.IsValid() == false)
			{
				return;
			}
			if (element.HasComponent<UIImageComponent>())
			{
				element.GetComponent<UIImageComponent>().Color = color;
			}
			if (element.HasComponent<UITextComponent>())
			{
				element.GetComponent<UITextComponent>().Color = color;
			}
		}

		// A number as up to three digit sprites, centred on `centre`; unused digits are hidden.
		void SetNumber(Scene& scene, const entt::entity (&digits)[3], int value, const glm::vec2& centre, float spacing, const glm::vec4& color)
		{
			const std::string text = std::to_string(std::clamp(value, 0, 999));
			const int count = (int)text.size();
			for (int d = 0; d < 3; d++)
			{
				if (d < count)
				{
					SetSprite(scene, digits[d], BattleIcon::DigitBase + (uint32_t)(text[d] - '0'));
					SetPosition(scene, digits[d], centre + glm::vec2(((float)d - 0.5f * (float)(count - 1)) * spacing, 0.0f));
					SetColor(scene, digits[d], color);
				}
				else
				{
					SetColor(scene, digits[d], glm::vec4(0.0f));
				}
			}
		}

		// ---- Presentation -----------------------------------------------------------------------------------

		void Present(Scene& scene)
		{
			const bool acting = m_Phase == Phase::Acting;
			const int actor = m_Action.Actor;

			// Heroes: where their strikes take them (UpdateHeroStrikes), a shake when hit.
			for (int h = 0; h < HeroCount; h++)
			{
				Hero& hero = m_Heroes[h];
				glm::vec2 offset = hero.Offset;
				if (hero.Flash > 0.0f)
				{
					offset.x += std::sin(m_Time * 70.0f) * 0.2f;
				}
				PlaceActor(scene, hero.Sprite, hero.Shadow, hero.Base + offset, 0.0f, HeroFoot, HeroPivot);
				const float heat = std::clamp(hero.Flash / 0.35f, 0.0f, 1.0f);
				SetColor(scene, hero.Sprite, glm::vec4(1.0f, 1.0f - 0.6f * heat, 1.0f - 0.6f * heat, 1.0f));

				// Her status: HP and MP as numbers and bars (sage, amber below half, red below a quarter), boost points.
				const HeroDefinition& definition = GetHero(h);
				const float hpFraction = (float)BattleState::HeroHP[h] / (float)definition.MaxHP;
				const glm::vec4 hpColor = hpFraction > 0.5f ? glm::vec4(0.5f, 0.72f, 0.4f, 1.0f) : hpFraction > 0.25f ? glm::vec4(0.92f, 0.72f, 0.3f, 1.0f) : glm::vec4(0.86f, 0.3f, 0.24f, 1.0f);
				UI::SetText(EntityOf(hero.StatusHP), std::to_string(BattleState::HeroHP[h]) + " / " + std::to_string(definition.MaxHP));
				UI::SetText(EntityOf(hero.StatusMP), std::to_string(BattleState::HeroMP[h]) + " / " + std::to_string(definition.MaxMP));
				SetFill(EntityOf(hero.StatusHPFill), hpFraction, hpColor);
				SetFill(EntityOf(hero.StatusMPFill), (float)BattleState::HeroMP[h] / (float)definition.MaxMP, glm::vec4(0.38f, 0.6f, 0.9f, 1.0f));
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					// Full points are gold; the ones the boost is about to spend pulse.
					const bool spending = m_Phase == Phase::Command && actor == h && p >= hero.Momentum - m_Action.Boost && p < hero.Momentum;
					const float pulse = spending ? 0.55f + 0.45f * std::sin(m_Time * 10.0f) : 1.0f;
					SetUIColor(EntityOf(hero.StatusPips[p]), p < hero.Momentum ? glm::vec4(1.0f, 0.8f * pulse + 0.2f, 0.42f * pulse, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 0.22f));
				}
			}

			// Enemies: the one in use is shown, the others hidden; a lunge when attacking, a fade when defeated.
			for (int e = 0; e < BattleState::MaxEnemies; e++)
			{
				Enemy& enemy = m_Enemies[e];
				const bool present = enemy.Kind != EnemyKind::None;
				if (present && enemy.HP <= 0)
				{
					enemy.Fade = std::max(0.0f, enemy.Fade - m_DeltaTime * 2.5f);
				}

				glm::vec2 offset{ 0.0f };
				if (acting && actor == HeroCount + e)
				{
					const float t = std::clamp((m_PhaseTime - 0.2f) / 0.6f, 0.0f, 1.0f);
					const glm::vec2 goal = m_Heroes[m_Action.Target].Base + glm::vec2(-3.0f, 0.0f);
					offset = (goal - enemy.Base) * std::sin(t * 3.1415927f) * 0.85f;
				}
				if (enemy.Flash > 0.0f)
				{
					offset.x += std::sin(m_Time * 70.0f) * 0.2f;
				}
				// A wisp floats (metres above the floor); its contact shadow stays on the ground.
				const float bob = enemy.Kind == EnemyKind::Wisp ? 0.35f + std::sin(m_Time * 2.2f + (float)e) * 0.12f : 0.0f;
				for (int s = 0; s < 2; s++)
				{
					if (Alive(scene, enemy.Shadows[s]) && scene.m_Registry.has<Shadow2DComponent>(enemy.Shadows[s]))
					{
						scene.m_Registry.get<Shadow2DComponent>(enemy.Shadows[s]).Strength = (int)enemy.Kind == s + 1 ? 0.45f * enemy.Fade : 0.0f;
					}
				}

				const float heat = std::clamp(enemy.Flash / 0.3f, 0.0f, 1.0f);
				const bool broken = IsBroken(enemy) && enemy.HP > 0;
				for (int s = 0; s < 2; s++)
				{
					const bool shown = present && (int)enemy.Kind == s + 1;
					PlaceActor(scene, enemy.Sprites[s], enemy.Shadows[s], enemy.Base + offset, s == 1 ? bob : 0.0f, EnemyFoot, EnemyPivot[s]);
					const float tint = broken ? 0.65f : 1.0f;
					SetColor(scene, enemy.Sprites[s], shown ? glm::vec4(tint + (1.0f - tint) * heat, tint * (1.0f - 0.6f * heat), tint * (1.0f - 0.6f * heat), enemy.Fade) : glm::vec4(0.0f));
				}
				if (Alive(scene, enemy.Light) && scene.m_Registry.has<Light2DComponent>(enemy.Light))
				{
					PlaceActor(scene, enemy.Light, entt::null, enemy.Base + offset, bob, 0.0f, 0.0f);
					scene.m_Registry.get<Light2DComponent>(enemy.Light).Intensity = enemy.Kind == EnemyKind::Wisp ? 1.1f * enemy.Fade * (0.85f + 0.15f * std::sin(m_Time * 3.0f)) : 0.0f;
				}

				// Its info under its feet (a World canvas): the shield (a "Break" label while broken), the weaknesses
				// found so far ("?" for the rest), HP.
				Entity info = EntityOf(enemy.Info);
				const bool showInfo = present && enemy.HP > 0;
				if (UI::IsVisible(info) != showInfo)
				{
					UI::SetVisible(info, showInfo);
				}
				if (showInfo && info.IsValid() && info.HasComponent<TransformComponent>())
				{
					scene.SetWorldPosition(info, GroundPoint(enemy.Base + offset));
				}
				if (showInfo)
				{
					SetFill(EntityOf(enemy.HPFill), (float)enemy.HP / (float)GetEnemy(enemy.Kind).MaxHP, glm::vec4(0.86f, 0.36f, 0.28f, 1.0f));
					SetUIColor(EntityOf(enemy.ShieldIcon), broken ? glm::vec4(0.75f, 0.32f, 0.28f, 1.0f) : glm::vec4(1.0f));
					UI::SetText(EntityOf(enemy.ShieldCount), broken ? std::string() : std::to_string(enemy.Shield));
					UI::SetText(EntityOf(enemy.WeakLabel), broken ? "BREAK" : "WEAK");
					SetUIColor(EntityOf(enemy.WeakLabel), broken ? glm::vec4(1.0f, 0.5f, 0.38f, 1.0f) : glm::vec4(1.0f, 0.84f, 0.5f, 1.0f));
					for (int w = 0; w < 3; w++)
					{
						const DamageType type = GetEnemy(enemy.Kind).Weak[w];
						const bool used = type != DamageType::None;
						const bool known = used && BattleState::Revealed[(int)enemy.Kind][w];
						Entity icon = EntityOf(enemy.WeakIcons[w]);
						if (UI::IsVisible(icon) != used)
						{
							UI::SetVisible(icon, used);
						}
						if (icon.IsValid() && icon.HasComponent<UIImageComponent>())
						{
							icon.GetComponent<UIImageComponent>().SpriteIndex = known ? BattleIcon::ForType(type) : BattleIcon::Unknown;
						}
					}
				}
			}

			// The hero whose turn it is, the target, the commands and the boost.
			const bool choosing = m_Phase == Phase::Command;
			if (Alive(scene, m_TurnMarker))
			{
				const bool show = (choosing || acting) && IsHero(actor);
				const Hero& marked = m_Heroes[std::min(actor, HeroCount - 1)];
				SetPosition(scene, m_TurnMarker, ScreenAbove(marked.Base + marked.Offset, HeroMarkerHeight) + glm::vec2(0.0f, 0.15f * std::sin(m_Time * 6.0f)));
				SetColor(scene, m_TurnMarker, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}
			if (Alive(scene, m_Cursor))
			{
				const bool show = choosing && m_Targeting && m_Action.Target >= 0 && m_Enemies[m_Action.Target].Alive();
				const glm::vec2 base = m_Action.Target >= 0 ? m_Enemies[m_Action.Target].Base : glm::vec2(0.0f);
				SetPosition(scene, m_Cursor, ScreenAbove(base, EnemyCursorHeight) + glm::vec2(0.0f, 0.15f * std::sin(m_Time * 8.0f)));
				SetColor(scene, m_Cursor, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}

			PresentCommandMenu(std::min(actor, HeroCount - 1), choosing);

			// The turn order (runtime UI, top left): from the one acting now (the big first chip) to the end of the
			// round, each chip showing its actor's sprite; enemies' chips are reddish. Unused chips are hidden.
			const int first = std::max(m_Turn, 0);
			int shown = 0;
			for (int index = first; index < (int)m_Order.size() && shown < 6; index++)
			{
				const int who = m_Order[index];
				if (ActorAlive(who) == false)
				{
					continue;
				}
				const entt::entity sprite = IsHero(who) ? m_Heroes[who].Sprite : m_Enemies[EnemyIndex(who)].Sprites[(int)m_Enemies[EnemyIndex(who)].Kind - 1];
				Entity chip = EntityOf(m_OrderChips[shown]);
				if (UI::IsVisible(chip) == false)
				{
					UI::SetVisible(chip, true);
				}
				const bool now = index == m_Turn;
				SetUIColor(chip, IsHero(who) ? (now ? glm::vec4(1.0f, 0.9f, 0.62f, 1.0f) : glm::vec4(1.0f)) : glm::vec4(1.0f, 0.72f, 0.66f, 1.0f));
				Entity portrait = EntityOf(m_OrderActors[shown]);
				if (portrait.IsValid() && portrait.HasComponent<UIImageComponent>() && Alive(scene, sprite) && scene.m_Registry.has<SpriteRendererComponent>(sprite))
				{
					const SpriteRendererComponent& renderer = scene.m_Registry.get<SpriteRendererComponent>(sprite);
					UIImageComponent& image = portrait.GetComponent<UIImageComponent>();
					image.Texture = renderer.Texture;
					image.SpriteIndex = renderer.SpriteIndex;
					image.Color = glm::vec4(1.0f);
				}
				shown++;
			}
			for (int c = shown; c < 6; c++)
			{
				Entity chip = EntityOf(m_OrderChips[c]);
				if (UI::IsVisible(chip))
				{
					UI::SetVisible(chip, false);
				}
			}

			PresentSpriteEffect(scene, m_StrikeEffect);
			PresentSpriteEffect(scene, m_HealEffect);

			// The message box (runtime UI), damage numbers and the fade.
			const bool ending = m_Phase == Phase::Victory || m_Phase == Phase::Defeat || m_Phase == Phase::Fleeing || m_Phase == Phase::Done;
			const bool showMessage = m_MessageTime > 0.0f || ending;
			Entity messagePanel = EntityOf(m_MessagePanel);
			if (UI::IsVisible(messagePanel) != showMessage)
			{
				UI::SetVisible(messagePanel, showMessage);
			}
			Entity messageText = EntityOf(m_MessageText);
			UI::SetText(messageText, GetLabelText(m_MessageLabel));
			if (messageText.IsValid() && messageText.HasComponent<UITextComponent>())
			{
				const glm::vec3 messageTint = m_MessageLabel == BattleLabel::Break ? glm::vec3(1.0f, 0.45f, 0.3f)
					: m_MessageLabel == BattleLabel::Weak ? glm::vec3(1.0f, 0.75f, 0.3f) : glm::vec3(0.95f, 0.9f, 0.78f);
				messageText.GetComponent<UITextComponent>().Color = glm::vec4(messageTint, 1.0f);
			}

			for (Popup& popup : m_Popups)
			{
				const float t = popup.Time / 0.9f;
				const float alpha = t < 1.0f ? std::min(1.0f, 3.0f * (1.0f - t)) : 0.0f;
				SetNumber(scene, popup.Digits, popup.Value, popup.Position + glm::vec2(0.0f, 0.9f * std::min(t, 1.0f)), 0.55f,
					glm::vec4(popup.Color.r, popup.Color.g, popup.Color.b, alpha));
			}

			float overlay = 0.0f;
			if (m_Phase == Phase::Intro)
			{
				overlay = 1.0f - std::min(1.0f, m_PhaseTime / 0.6f);
			}
			else if (m_Phase == Phase::Victory || m_Phase == Phase::Defeat)
			{
				overlay = std::clamp((m_PhaseTime - 1.2f) / 0.7f, 0.0f, 1.0f);
			}
			else if (m_Phase == Phase::Fleeing)
			{
				overlay = std::clamp((m_PhaseTime - 0.3f) / 0.5f, 0.0f, 1.0f);
			}
			else if (m_Phase == Phase::Done)
			{
				overlay = 1.0f;
			}
			SetColor(scene, m_Overlay, m_Phase == Phase::Defeat ? glm::vec4(0.35f, 0.0f, 0.0f, overlay) : glm::vec4(0.0f, 0.0f, 0.0f, overlay));

			ApplyLayout(scene);
		}

		Phase m_Phase = Phase::Intro;
		float m_PhaseTime = 0.0f;
		float m_Time = 0.0f;
		float m_DeltaTime = 0.0f;
		SpriteEffect m_StrikeEffect;
		SpriteEffect m_HealEffect;

		// A potion is used this far into its (BattleIdle) clip; the heal sprite's clip lasts HealEffectTime.
		static constexpr float ItemTime = 0.35f;
		static constexpr float HealEffectTime = 0.66f;
		static constexpr float ShakeTime = 0.22f;
		float m_ShakeTime = 0.0f;
		float m_ShakeAmount = 0.0f;
		entt::entity m_Camera = entt::null;
		StageCamera m_Stage;
		std::unordered_map<entt::entity, LayoutItem> m_Layout;
		int m_Round = 0;
		int m_Turn = -1;
		int m_Target = 0;   // the last target, kept for the next hero
		std::vector<int> m_Order;
		Action m_Action;
		std::array<Hero, HeroCount> m_Heroes;
		std::array<Enemy, BattleState::MaxEnemies> m_Enemies;
		std::array<Popup, 3> m_Popups;
		int m_NextPopup = 0;
		BattleLabel m_MessageLabel = BattleLabel::Attack;
		float m_MessageTime = 0.0f;
		bool m_Previous[14] = {};
		std::mt19937 m_Random{ std::random_device{}() };

		// The command menu and the message box (runtime UI under "Battle UI Canvas").
		entt::entity m_Commands[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_CommandNames[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_CommandCosts[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_CommandPanel = entt::null;
		entt::entity m_CommandHeader = entt::null;
		entt::entity m_CommandBoost = entt::null;
		entt::entity m_CommandHelp = entt::null;
		int m_HelpCommand = 0;              // the command the help line describes (the focused one)
		entt::entity m_MessagePanel = entt::null;
		entt::entity m_MessageText = entt::null;
		entt::entity m_TargetPanel = entt::null;
		entt::entity m_TargetHeader = entt::null;
		entt::entity m_TargetButtons[BattleState::MaxEnemies] = {};
		entt::entity m_TargetNames[BattleState::MaxEnemies] = {};
		entt::entity m_TargetHP[BattleState::MaxEnemies] = {};
		int m_ChosenCommand = -1;           // submitted by the menu, used by the next UpdateCommand
		int m_LastCommand[HeroCount] = {};  // where each hero's menu opens
		bool m_Targeting = false;           // the target list is open for m_TargetCommand
		int m_TargetCommand = -1;
		int m_ChosenTarget = -1;            // submitted by the target list
		bool m_CancelTarget = false;        // Cancel in the target list: back to the commands
		bool m_CommandsShown = false;
		bool m_TargetsShown = false;
		entt::entity m_OrderChips[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_OrderActors[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_Cursor = entt::null;
		entt::entity m_TurnMarker = entt::null;
		entt::entity m_Overlay = entt::null;
	};
}
