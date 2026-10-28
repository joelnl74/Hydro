#pragma once
#include "Hydro.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Native scripts of the Diorama RPG battle prototype (docs/DioramaRPG.md): enemies that wander and chase the
// player in the village (Assets/Scenes/StatueVillage.hyscene: slimes in the Slime Field, spirit wisps in the
// meadow and the field), and the party battle they start (Assets/Scenes/Battle.hyscene). The village is pushed
// down on the scene stack while the battle runs (SceneTransition) and carries on when it ends.
//
// The party is the shrine maiden Miko alone (HeroCount): the hero table, the scene's "Hero <Name>" entities and
// the sheets in Assets/DioramaRPG/Battle come from scripts/shrine_glade/battle.py. The code is written for any
// HeroCount, so a second hero is a row in GetHero, its entities in the scene and HeroCount + 1.
//
// Quick prototype code for play testing: the rules, the data and the presentation live together here. There
// is no text rendering yet, so the labels, icons and numbers are sprites from Assets/DioramaRPG/UI.
//
// Battle keys: 1 attack, 2 and 3 the hero's skills, 4 guard, 5 potion, 6 run; Left / Right (A / D) pick the
// target; Up / Down (W / S) set the boost (spend Momentum: more hits or a stronger skill).
namespace Hydro
{
	// ---------------------------------------------------------------------------------------------------------
	// Data

	enum class DamageType : uint8_t { Slash = 0, Pierce, Blunt, Fire, Light, None };

	enum class EnemyKind : uint8_t { None = 0, Slime, Wisp };

	// Rows of Assets/DioramaRPG/UI/BattleText.png (keep in step with scripts/shrine_glade/battle.py TEXT_ROWS and
	// NEW_TEXT, which appends the last three rows from the sheet's own glyphs).
	enum class BattleLabel : uint32_t
	{
		Attack = 0, FlameEdge, Crush, HolyLight, Heal, Guard, Potion, Run, Orin, Lila, Slime, Wisp,
		Weak, Resist, Break, Victory, Defeated, Escaped, Boost, NoMP, NoPotions, Critical, Guarding, NoEscape,
		Miko, FlameKick, HolyPalm
	};

	// Cells of Assets/DioramaRPG/UI/BattleIcons.png: row Icon, then row Digit (0-9 from DigitBase).
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
		// Battle sheet clips (DioramaRPG/Battle/<Name>_Battle.hyanim) for the two skills and for using an item.
		const char* SkillClip[2];
		const char* ItemClip;
	};

	// The party: Miko, the shrine maiden, fighting bare-handed (Blunt). Flame Kick is the spinning strike of her
	// Attack clip (contact at FirstHitTime); Holy Palm the palm thrust of her Cast clip (its peak at SpellPeakTime).
	// Alone she carries the party's HP and MP, so she is sturdier than Orin and Lila were on their own.
	constexpr int HeroCount = 1;

	inline const HeroDefinition& GetHero(int index)
	{
		static const HeroDefinition heroes[HeroCount] = {
			{ "Miko", BattleLabel::Miko, 90, 32, 10, 8, 9, DamageType::Blunt,
				{ BattleLabel::FlameKick, BattleLabel::HolyPalm }, { DamageType::Fire, DamageType::Light }, { 5, 6 }, { 1.6f, 1.7f }, { false, false }, { "Attack", "Cast" }, "Cast" },
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
	// Put it on a Dynamic Rigidbody2D (GravityScale 0, high LinearDamping, FixedRotation) with a Box collider.
	HY_SCRIPT()
	class EnemyChaseScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			const glm::vec3& position = GetComponent<TransformComponent>().Translation;
			m_Home = { position.x, position.y };
			m_Target = m_Home;
			m_Seed = 2463534242u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 7);
			m_Wait = 0.5f + 2.0f * Random01();
			if (GetEntity().HasComponent<TagComponent>() && GetComponent<TagComponent>().Tag.rfind("Wisp", 0) == 0)
			{
				m_Kind = EnemyKind::Wisp;
			}
			BattleState::Reset();
		}

		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();
			m_Time += deltaTime;

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

			auto& transform = GetComponent<TransformComponent>();
			const glm::vec2 position{ transform.Translation.x, transform.Translation.y };
			m_Cooldown = std::max(0.0f, m_Cooldown - deltaTime);

			if (m_Player == entt::null || scene->m_Registry.valid(m_Player) == false)
			{
				m_Player = FindPlayer(*scene);
			}

			float playerDistance = 1.0e9f;
			if (m_Player != entt::null && scene->m_Registry.has<TransformComponent>(m_Player))
			{
				const glm::vec3& playerPosition = scene->m_Registry.get<TransformComponent>(m_Player).Translation;
				m_PlayerPosition = { playerPosition.x, playerPosition.y };
				playerDistance = glm::length(m_PlayerPosition - position);

				// A player without a physics body (DioramaWalk in the Shrine Glade) never collides: touching
				// is being this close on the same level.
				if (scene->m_Registry.has<Rigidbody2DComponent>(m_Player) == false && playerDistance < TouchRange
					&& std::abs(playerPosition.z - transform.Translation.z) < 0.5f)
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
				transform.Translation.x += velocity.x * deltaTime;
				transform.Translation.y += velocity.y * deltaTime;
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

		static constexpr float AggroRange = 5.5f;   // the player this close wakes it up
		static constexpr float TouchRange = 0.7f;   // a player without a body this close is touched
		static constexpr float LeashRange = 10.0f;  // and this far makes it give up
		static constexpr float MaxFromHome = 16.0f;
		static constexpr float WanderRadius = 3.0f;
		static constexpr float WanderSpeed = 0.8f;
		static constexpr float ChaseSpeed = 2.2f;   // the player walks at 3
		static constexpr float ReturnSpeed = 1.8f;

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
	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			for (int h = 0; h < HeroCount; h++)
			{
				const std::string prefix = std::string("Hero ") + GetHero(h).Name;
				Hero& hero = m_Heroes[h];
				hero.Sprite = Find(scene, prefix);
				hero.HPBack = Find(scene, prefix + " HP Back");
				hero.HPFill = Find(scene, prefix + " HP Fill");
				hero.MPBack = Find(scene, prefix + " MP Back");
				hero.MPFill = Find(scene, prefix + " MP Fill");
				for (int d = 0; d < 3; d++)
				{
					hero.HPDigits[d] = Find(scene, prefix + " HP Digit " + std::to_string(d));
				}
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					hero.Pips[p] = Find(scene, prefix + " Pip " + std::to_string(p));
				}
				hero.Base = Position(scene, hero.Sprite);
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
				enemy.Light = Find(scene, prefix + " Light");
				enemy.HPBack = Find(scene, prefix + " HP Back");
				enemy.HPFill = Find(scene, prefix + " HP Fill");
				enemy.ShieldIcon = Find(scene, prefix + " Shield");
				enemy.ShieldDigit = Find(scene, prefix + " Shield Digit");
				for (int w = 0; w < 3; w++)
				{
					enemy.WeakIcons[w] = Find(scene, prefix + " Weak " + std::to_string(w));
				}
				enemy.Base = Position(scene, enemy.Sprites[0]);
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
				m_CommandKeys[c] = Find(scene, "Command Key " + std::to_string(c));
				m_OrderChips[c] = Find(scene, "Order Chip " + std::to_string(c));
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
			m_BoostLabel = Find(scene, "Boost Label");
			m_BoostDigit = Find(scene, "Boost Digit");
			m_Message = Find(scene, "Message");
			m_Overlay = Find(scene, "Battle Overlay");
			m_SlashEffect.Entity = Find(scene, "FX Slash");
			m_HealEffect.Entity = Find(scene, "FX Heal");

			// Keys held when the battle opens (the player was still walking) do not count.
			for (bool& previous : m_Previous)
			{
				previous = true;
			}

			HY_INFO("Battle: {} enemies! (1 attack, 2/3 skills, 4 guard, 5 potion, 6 run; Left/Right target, Up/Down boost)", count);
			SetPhase(Phase::Intro);
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			m_DeltaTime = deltaTime;
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

	private:
		enum class Phase : uint8_t { Intro, NextTurn, Command, EnemyThink, Acting, Victory, Defeat, Fleeing, Done };
		enum class Command : uint8_t { Attack = 0, SkillA, SkillB, Guard, Potion, Run, EnemyAttack };

		struct Hero
		{
			entt::entity Sprite = entt::null;
			entt::entity HPBack = entt::null, HPFill = entt::null, MPBack = entt::null, MPFill = entt::null;
			entt::entity HPDigits[3] = { entt::null, entt::null, entt::null };
			entt::entity Pips[BattleState::MaxMomentum] = { entt::null, entt::null, entt::null, entt::null, entt::null };
			glm::vec2 Base{ 0.0f };
			int Momentum = 1;
			bool BoostedLastRound = false;
			bool Guarding = false;
			float Flash = 0.0f;
		};

		struct Enemy
		{
			EnemyKind Kind = EnemyKind::None;
			entt::entity Sprites[2] = { entt::null, entt::null }; // slime, wisp
			entt::entity Light = entt::null;
			entt::entity HPBack = entt::null, HPFill = entt::null, ShieldIcon = entt::null, ShieldDigit = entt::null;
			entt::entity WeakIcons[3] = { entt::null, entt::null, entt::null };
			glm::vec2 Base{ 0.0f };
			int HP = 0;
			int Shield = 0;
			int BrokenUntilRound = -1; // broken while m_Round <= this
			float Flash = 0.0f;
			float Fade = 1.0f;         // 1 alive, falls to 0 when defeated
			float ShadowBaseY[2] = { -1000.0f, -1000.0f }; // the shadows' authored offsets (slime, wisp)
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
		};

		// ---- Flow -----------------------------------------------------------------------------------------

		void SetPhase(Phase phase)
		{
			m_Phase = phase;
			m_PhaseTime = 0.0f;
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

			if (Pressed(6, KeyCode::Left) || Pressed(7, KeyCode::A))
			{
				m_Action.Target = StepTarget(m_Action.Target, -1);
			}
			if (Pressed(8, KeyCode::Right) || Pressed(9, KeyCode::D))
			{
				m_Action.Target = StepTarget(m_Action.Target, 1);
			}
			if (Pressed(10, KeyCode::Up) || Pressed(11, KeyCode::W))
			{
				m_Action.Boost = std::min({ 3, hero.Momentum, m_Action.Boost + 1 });
			}
			if (Pressed(12, KeyCode::Down) || Pressed(13, KeyCode::S))
			{
				m_Action.Boost = std::max(0, m_Action.Boost - 1);
			}

			const KeyCode keys[6] = { KeyCode::D1, KeyCode::D2, KeyCode::D3, KeyCode::D4, KeyCode::D5, KeyCode::D6 };
			for (int c = 0; c < 6; c++)
			{
				if (Pressed(c, keys[c]) == false)
				{
					continue;
				}

				const Command command = (Command)c;
				if (command == Command::SkillA || command == Command::SkillB)
				{
					const int s = command == Command::SkillA ? 0 : 1;
					if (BattleState::HeroMP[h] < definition.SkillCost[s])
					{
						ShowMessage(BattleLabel::NoMP);
						continue;
					}
					BattleState::HeroMP[h] -= definition.SkillCost[s];
					if (definition.SkillHeals[s])
					{
						m_Action.Target = MostHurtHero();
					}
				}
				else if (command == Command::Potion)
				{
					if (m_Potions <= 0)
					{
						ShowMessage(BattleLabel::NoPotions);
						continue;
					}
					m_Potions--;
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
				return;
			}
		}

		void BeginHeroAction()
		{
			const int h = m_Action.Actor;
			m_StrikesPlayed = 0;
			Entity sprite = EntityOf(m_Heroes[h].Sprite);
			switch (m_Action.Kind)
			{
				case Command::Attack:
					SpriteAnimator::Play(sprite, "Attack", true);
					break;
				case Command::SkillA:
				case Command::SkillB:
				case Command::Potion:
				{
					const HeroDefinition& definition = GetHero(h);
					const char* clip = m_Action.Kind == Command::Potion ? definition.ItemClip : definition.SkillClip[m_Action.Kind == Command::SkillA ? 0 : 1];
					SpriteAnimator::Play(sprite, clip, true);
					ShowMessage(m_Action.Kind == Command::Potion ? BattleLabel::Potion : GetHero(h).SkillLabel[m_Action.Kind == Command::SkillA ? 0 : 1], 0.9f);
					break;
				}
				case Command::Guard:
					m_Heroes[h].Guarding = true;
					SpriteAnimator::Play(sprite, "Guard", true);
					ShowMessage(BattleLabel::Guarding, 0.7f);
					break;
				default:
					break;
			}
			SetPhase(Phase::Acting);
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
			if (hero && m_Action.Kind == Command::Attack)
			{
				// Lunge, then the hits HitSpacing apart, then back. The first hit lands on the Attack clip's contact
				// frame (anticipation, then the swing); each extra hit of a boost replays the swing alone (Strike),
				// started so its contact frame meets the hit.
				const float firstHit = FirstHitTime;
				const int next = m_Action.HitsDone;
				if (next > 0 && next < m_Action.Hits && m_StrikesPlayed < next && m_PhaseTime >= firstHit + HitSpacing * (float)next - StrikeContact)
				{
					SpriteAnimator::Play(EntityOf(m_Heroes[m_Action.Actor].Sprite), "Strike", true);
					m_StrikesPlayed = next;
				}
				while (m_Action.HitsDone < m_Action.Hits && m_PhaseTime >= firstHit + HitSpacing * (float)m_Action.HitsDone)
				{
					if (m_Enemies[m_Action.Target].Alive() == false)
					{
						m_Action.Target = FirstLivingEnemy(m_Action.Target);
						if (m_Action.Target < 0)
						{
							break;
						}
					}
					HitEnemy(m_Action.Target, GetHero(m_Action.Actor).WeaponType, (float)GetHero(m_Action.Actor).Attack, 1.0f);
					m_Action.HitsDone++;
				}
				const float end = firstHit + HitSpacing * (float)(m_Action.Hits - 1) + 0.55f;
				if (m_PhaseTime >= end || m_Action.Target < 0)
				{
					EndAction();
				}
				return;
			}

			if (hero && (m_Action.Kind == Command::SkillA || m_Action.Kind == Command::SkillB))
			{
				// Skills on the Attack clip (a strike) land at FirstHitTime; spells on the Cast / Heal clip at its peak.
				const HeroDefinition& skillHero = GetHero(m_Action.Actor);
				const char* skillClip = skillHero.SkillClip[m_Action.Kind == Command::SkillA ? 0 : 1];
				const float resolveAt = std::string(skillClip) == "Attack" ? FirstHitTime : SpellPeakTime;
				if (m_Action.HitsDone == 0 && m_PhaseTime >= resolveAt)
				{
					m_Action.HitsDone = 1;
					const HeroDefinition& definition = skillHero;
					const int s = m_Action.Kind == Command::SkillA ? 0 : 1;
					const float boost = 1.0f + 0.6f * (float)m_Action.Boost;
					if (definition.SkillHeals[s])
					{
						HealHero(m_Action.Target, (int)std::lround(definition.SkillPower[s] * (float)definition.Magic * boost * (0.9f + 0.2f * Random01())));
					}
					else
					{
						if (m_Enemies[m_Action.Target].Alive() == false)
						{
							m_Action.Target = FirstLivingEnemy(m_Action.Target);
						}
						if (m_Action.Target >= 0)
						{
							const float stat = definition.SkillType[s] == DamageType::Light || definition.SkillType[s] == DamageType::Fire
								? (float)std::max(definition.Magic, definition.Attack) : (float)definition.Attack;
							HitEnemy(m_Action.Target, definition.SkillType[s], stat, definition.SkillPower[s] * boost);
						}
					}
				}
				if (m_PhaseTime >= 1.2f)
				{
					EndAction();
				}
				return;
			}

			if (hero && m_Action.Kind == Command::Potion)
			{
				if (m_Action.HitsDone == 0 && m_PhaseTime >= SpellPeakTime)
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
				SpawnPopup(target.Base + glm::vec2(0.0f, 4.5f), damage, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				PlayEffect("Effect Hit Sparks", target.Base + glm::vec2(0.0f, 2.0f), HitSparks(DamageType::Blunt, target.Guarding == false));
				Entity sprite = EntityOf(target.Sprite);
				if (BattleState::HeroHP[m_Action.Target] <= 0)
				{
					SpriteAnimator::Play(sprite, "Down", true);
					HY_INFO("Battle: {} falls!", GetHero(m_Action.Target).Name);
				}
				else if (target.Guarding == false)
				{
					SpriteAnimator::Play(sprite, "Hurt", true);
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
				if (m_Heroes[m_Action.Actor].Guarding == false)
				{
					SpriteAnimator::Play(sprite, "Idle");
				}
			}
			for (int h = 0; h < HeroCount; h++)
			{
				if (BattleState::HeroHP[h] > 0 && m_Heroes[h].Guarding == false && (h != m_Action.Actor || IsHero(m_Action.Actor) == false))
				{
					SpriteAnimator::Play(EntityOf(m_Heroes[h].Sprite), "Idle");
				}
			}
			SetPhase(Phase::NextTurn);
		}

		void HitEnemy(int index, DamageType type, float stat, float power)
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

			SpawnPopup(enemy.Base + glm::vec2(0.0f, 3.2f), damage, color);
			PlayEffect("Effect Hit Sparks", enemy.Base + glm::vec2(0.0f, 1.2f), HitSparks(type, weak || critical));
			PlaySpriteEffect(m_SlashEffect, enemy.Base + glm::vec2(0.2f, 1.6f), "Slash", SlashEffectTime);
			HY_INFO("Battle: {} takes {}{}{}{} ({} HP left, shield {})", definition.Name, damage, weak ? " (weak)" : "", resisted ? " (resisted)" : "",
				critical ? " (critical)" : "", enemy.HP, enemy.Shield);
			if (enemy.HP <= 0)
			{
				HY_INFO("Battle: {} is defeated", definition.Name);
				PlayEffect("Effect Defeat Puff", enemy.Base + glm::vec2(0.0f, 1.0f), DefeatPuff(enemy.Kind));
			}
		}

		// One-shot effect sprites (Battle.hyscene: "FX Slash", "FX Heal"): placed, restarted, shown while their clip runs;
		// a point light on the same entity flares with it (PresentSpriteEffect fades both). They play alongside the
		// particle effects below: the sprite gives the authored shape, the particles the spray.
		struct SpriteEffect
		{
			entt::entity Entity = entt::null;
			float Time = 0.0f;     // seconds left
			float Duration = 1.0f;
			float LightBase = 0.0f;
		};

		void PlaySpriteEffect(SpriteEffect& effect, const glm::vec2& position, const char* clip, float duration)
		{
			Scene& scene = *GetEntity().GetScene();
			if (Alive(scene, effect.Entity) == false)
			{
				return;
			}
			SetPosition(scene, effect.Entity, position);
			SpriteAnimator::Play(EntityOf(effect.Entity), clip, true);
			effect.Time = duration;
			effect.Duration = duration;
		}

		void PresentSpriteEffect(Scene& scene, SpriteEffect& effect)
		{
			if (Alive(scene, effect.Entity) == false)
			{
				return;
			}
			effect.Time = std::max(0.0f, effect.Time - m_DeltaTime);
			const float k = effect.Time > 0.0f ? std::min(1.0f, effect.Time / (0.5f * effect.Duration)) : 0.0f;
			SetColor(scene, effect.Entity, glm::vec4(1.0f, 1.0f, 1.0f, effect.Time > 0.0f ? 1.0f : 0.0f));
			if (scene.m_Registry.has<Light2DComponent>(effect.Entity))
			{
				Light2DComponent& light = scene.m_Registry.get<Light2DComponent>(effect.Entity);
				if (effect.LightBase <= 0.0f)
				{
					effect.LightBase = std::max(light.Intensity, 0.8f);
				}
				light.Intensity = effect.LightBase * k;
			}
		}

		void HealHero(int h, int amount)
		{
			if (BattleState::HeroHP[h] <= 0)
			{
				return;
			}
			const int before = BattleState::HeroHP[h];
			BattleState::HeroHP[h] = std::min(GetHero(h).MaxHP, BattleState::HeroHP[h] + amount);
			SpawnPopup(m_Heroes[h].Base + glm::vec2(0.0f, 4.5f), BattleState::HeroHP[h] - before, glm::vec4(0.45f, 1.0f, 0.55f, 1.0f));
			PlayEffect("Effect Heal", m_Heroes[h].Base + glm::vec2(0.0f, 0.8f), HealMotes());
			PlaySpriteEffect(m_HealEffect, m_Heroes[h].Base + glm::vec2(0.0f, 1.2f), "Heal", HealEffectTime);
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

		// Sparks where a hit lands, coloured by the damage type. Unlit, so they stay bright in a dark battle.
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
			sparks.Lighting = ParticleLighting::Unlit;
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

		// Green motes rising from a healed hero. Unlit.
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
			motes.Lighting = ParticleLighting::Unlit;
			motes.SortOrder = 200;
			motes.DestroyWhenFinished = true;
			return motes;
		}

		// Creates the effect entity at `position`. Creating entities is allowed while scripts run; the
		// emitter starts in this frame's particle stage.
		void PlayEffect(const char* name, const glm::vec2& position, const ParticleEmitter2DComponent& settings)
		{
			Entity effect = GetEntity().GetScene()->CreateEntity(name);
			effect.AddComponent<TransformComponent>().Translation = { position.x, position.y, 0.0f };
			effect.AddComponent<ParticleEmitter2DComponent>(settings);
		}

		bool Pressed(int index, KeyCode key)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && m_Previous[index] == false;
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

		static glm::vec2 Position(Scene& scene, entt::entity entity)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				const glm::vec3& position = scene.m_Registry.get<TransformComponent>(entity).Translation;
				return { position.x, position.y };
			}

			return glm::vec2(0.0f);
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

		static void SetPosition(Scene& scene, entt::entity entity, const glm::vec2& position)
		{
			if (Alive(scene, entity))
			{
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
			}
		}

		// A bar made of two quads: the back one is as wide as a full bar, the fill sticks to its left end.
		static void SetBar(Scene& scene, entt::entity back, entt::entity fill, float fraction, const glm::vec4& color, float alpha)
		{
			if (Alive(scene, back) == false || Alive(scene, fill) == false)
			{
				return;
			}

			const TransformComponent& backTransform = scene.m_Registry.get<TransformComponent>(back);
			TransformComponent& fillTransform = scene.m_Registry.get<TransformComponent>(fill);
			const float inset = 0.12f;
			const float width = backTransform.Scale.x - inset;
			const float length = std::max(0.0f, width * std::clamp(fraction, 0.0f, 1.0f));
			fillTransform.Scale.x = std::max(0.001f, length);
			fillTransform.Scale.y = backTransform.Scale.y - inset;
			fillTransform.Translation.x = backTransform.Translation.x - 0.5f * width + 0.5f * length;
			fillTransform.Translation.y = backTransform.Translation.y;
			SetColor(scene, back, glm::vec4(0.08f, 0.07f, 0.05f, alpha));
			SetColor(scene, fill, glm::vec4(color.r, color.g, color.b, length > 0.0f ? alpha : 0.0f));
		}

		// A number as up to three digit sprites, centred on `centre`; unused digits are hidden.
		static void SetNumber(Scene& scene, const entt::entity (&digits)[3], int value, const glm::vec2& centre, float spacing, const glm::vec4& color)
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

			// Heroes: lunge on attacks, shake when hit.
			for (int h = 0; h < HeroCount; h++)
			{
				Hero& hero = m_Heroes[h];
				glm::vec2 offset{ 0.0f };
				if (acting && actor == h && m_Action.Kind == Command::Attack && m_Action.Target >= 0)
				{
					const float total = FirstHitTime + HitSpacing * (float)(m_Action.Hits - 1) + 0.55f;
					const float t = std::clamp(m_PhaseTime / total, 0.0f, 1.0f);
					const float reach = std::sin(std::min(1.0f, t * 1.15f) * 3.1415927f);
					const glm::vec2 goal = m_Enemies[m_Action.Target].Base + glm::vec2(3.2f, 0.0f);
					offset = (goal - hero.Base) * std::min(1.0f, reach * 1.2f);
				}
				if (hero.Flash > 0.0f)
				{
					offset.x += std::sin(m_Time * 70.0f) * 0.2f;
				}
				SetPosition(scene, hero.Sprite, hero.Base + offset);
				const float heat = std::clamp(hero.Flash / 0.35f, 0.0f, 1.0f);
				SetColor(scene, hero.Sprite, glm::vec4(1.0f, 1.0f - 0.6f * heat, 1.0f - 0.6f * heat, 1.0f));

				const HeroDefinition& definition = GetHero(h);
				const float hpFraction = (float)BattleState::HeroHP[h] / (float)definition.MaxHP;
				const glm::vec4 hpColor = hpFraction > 0.5f ? glm::vec4(0.45f, 0.85f, 0.35f, 1.0f) : hpFraction > 0.25f ? glm::vec4(0.95f, 0.78f, 0.25f, 1.0f) : glm::vec4(0.9f, 0.25f, 0.2f, 1.0f);
				SetBar(scene, hero.HPBack, hero.HPFill, hpFraction, hpColor, 1.0f);
				SetBar(scene, hero.MPBack, hero.MPFill, (float)BattleState::HeroMP[h] / (float)definition.MaxMP, glm::vec4(0.35f, 0.6f, 0.95f, 1.0f), 1.0f);
				const glm::vec2 hpBack = Position(scene, hero.HPBack);
				const float barWidth = Alive(scene, hero.HPBack) ? scene.m_Registry.get<TransformComponent>(hero.HPBack).Scale.x : 2.0f;
				SetNumber(scene, hero.HPDigits, BattleState::HeroHP[h], hpBack + glm::vec2(0.5f * barWidth + 0.75f, 0.0f), 0.42f, glm::vec4(1.0f));
				for (int p = 0; p < BattleState::MaxMomentum; p++)
				{
					const bool spending = m_Phase == Phase::Command && actor == h && p >= hero.Momentum - m_Action.Boost && p < hero.Momentum;
					SetSprite(scene, hero.Pips[p], p < hero.Momentum ? BattleIcon::PipFull : BattleIcon::PipEmpty);
					const float pulse = spending ? 0.55f + 0.45f * std::sin(m_Time * 10.0f) : 1.0f;
					SetColor(scene, hero.Pips[p], glm::vec4(1.0f, pulse, pulse, 1.0f));
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
				float bob = 0.0f;
				if (enemy.Kind == EnemyKind::Wisp)
				{
					bob = std::sin(m_Time * 2.2f + (float)e) * 0.25f;
					offset.y += bob;
				}
				// The contact shadow stays on the ground: it does not follow the float.
				for (int s = 0; s < 2; s++)
				{
					if (Alive(scene, enemy.Sprites[s]) && scene.m_Registry.has<Shadow2DComponent>(enemy.Sprites[s]))
					{
						Shadow2DComponent& shadow = scene.m_Registry.get<Shadow2DComponent>(enemy.Sprites[s]);
						if (enemy.ShadowBaseY[s] < -100.0f)
						{
							enemy.ShadowBaseY[s] = shadow.Offset.y;
						}
						shadow.Offset.y = enemy.ShadowBaseY[s] - bob;
						shadow.Strength = (int)enemy.Kind == s + 1 ? 0.45f * enemy.Fade : 0.0f;
					}
				}

				const float heat = std::clamp(enemy.Flash / 0.3f, 0.0f, 1.0f);
				const bool broken = IsBroken(enemy) && enemy.HP > 0;
				for (int s = 0; s < 2; s++)
				{
					const bool shown = present && (int)enemy.Kind == s + 1;
					SetPosition(scene, enemy.Sprites[s], enemy.Base + offset);
					const float tint = broken ? 0.65f : 1.0f;
					SetColor(scene, enemy.Sprites[s], shown ? glm::vec4(tint + (1.0f - tint) * heat, tint * (1.0f - 0.6f * heat), tint * (1.0f - 0.6f * heat), enemy.Fade) : glm::vec4(0.0f));
				}
				if (Alive(scene, enemy.Light) && scene.m_Registry.has<Light2DComponent>(enemy.Light))
				{
					SetPosition(scene, enemy.Light, enemy.Base + offset + glm::vec2(0.0f, 1.6f));
					scene.m_Registry.get<Light2DComponent>(enemy.Light).Intensity = enemy.Kind == EnemyKind::Wisp ? 1.1f * enemy.Fade * (0.85f + 0.15f * std::sin(m_Time * 3.0f)) : 0.0f;
				}

				const float uiAlpha = present && enemy.HP > 0 ? 1.0f : 0.0f;
				const float hpFraction = present ? (float)enemy.HP / (float)GetEnemy(enemy.Kind).MaxHP : 0.0f;
				SetBar(scene, enemy.HPBack, enemy.HPFill, hpFraction, glm::vec4(0.85f, 0.3f, 0.25f, 1.0f), uiAlpha);
				SetSprite(scene, enemy.ShieldIcon, broken ? BattleIcon::Broken : BattleIcon::Shield);
				SetColor(scene, enemy.ShieldIcon, glm::vec4(1.0f, 1.0f, 1.0f, uiAlpha));
				SetSprite(scene, enemy.ShieldDigit, BattleIcon::DigitBase + (uint32_t)std::clamp(enemy.Shield, 0, 9));
				SetColor(scene, enemy.ShieldDigit, glm::vec4(1.0f, 1.0f, 1.0f, broken ? 0.0f : uiAlpha));
				for (int w = 0; w < 3; w++)
				{
					const DamageType type = present ? GetEnemy(enemy.Kind).Weak[w] : DamageType::None;
					const bool used = type != DamageType::None;
					const bool known = used && BattleState::Revealed[(int)enemy.Kind][w];
					SetSprite(scene, enemy.WeakIcons[w], known ? BattleIcon::ForType(type) : BattleIcon::Unknown);
					SetColor(scene, enemy.WeakIcons[w], glm::vec4(1.0f, 1.0f, 1.0f, used ? uiAlpha : 0.0f));
				}
			}

			// The hero whose turn it is, the target, the commands and the boost.
			const bool choosing = m_Phase == Phase::Command;
			if (Alive(scene, m_TurnMarker))
			{
				const bool show = (choosing || acting) && IsHero(actor);
				SetPosition(scene, m_TurnMarker, m_Heroes[std::min(actor, HeroCount - 1)].Base + glm::vec2(0.0f, 6.3f + 0.15f * std::sin(m_Time * 6.0f)));
				SetColor(scene, m_TurnMarker, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}
			if (Alive(scene, m_Cursor))
			{
				const bool show = choosing && m_Action.Target >= 0 && m_Enemies[m_Action.Target].Alive();
				const glm::vec2 base = m_Action.Target >= 0 ? m_Enemies[m_Action.Target].Base : glm::vec2(0.0f);
				SetPosition(scene, m_Cursor, base + glm::vec2(0.0f, 3.4f + 0.15f * std::sin(m_Time * 8.0f)));
				SetColor(scene, m_Cursor, glm::vec4(1.0f, 1.0f, 1.0f, show ? 1.0f : 0.0f));
			}

			const int hero = std::min(actor, HeroCount - 1);
			const HeroDefinition& definition = GetHero(hero);
			const BattleLabel labels[6] = { BattleLabel::Attack, definition.SkillLabel[0], definition.SkillLabel[1], BattleLabel::Guard, BattleLabel::Potion, BattleLabel::Run };
			for (int c = 0; c < 6; c++)
			{
				SetSprite(scene, m_Commands[c], (uint32_t)labels[c]);
				bool usable = true;
				if (c == 1 || c == 2)
				{
					usable = BattleState::HeroMP[hero] >= definition.SkillCost[c - 1];
				}
				if (c == 4)
				{
					usable = m_Potions > 0;
				}
				const float strength = choosing ? (usable ? 1.0f : 0.35f) : 0.3f;
				SetColor(scene, m_Commands[c], glm::vec4(strength, strength, strength, 1.0f));
				SetSprite(scene, m_CommandKeys[c], BattleIcon::DigitBase + (uint32_t)(c + 1));
				SetColor(scene, m_CommandKeys[c], glm::vec4(1.0f, 0.85f, 0.45f, choosing ? 1.0f : 0.3f));
			}
			SetColor(scene, m_BoostLabel, glm::vec4(1.0f, 0.75f, 0.3f, choosing && m_Action.Boost > 0 ? 1.0f : 0.0f));
			SetSprite(scene, m_BoostDigit, BattleIcon::DigitBase + (uint32_t)m_Action.Boost);
			SetColor(scene, m_BoostDigit, glm::vec4(1.0f, 0.75f, 0.3f, choosing && m_Action.Boost > 0 ? 1.0f : 0.0f));

			// The turn order of this round: the one acting is bright, those who acted are dim.
			for (int c = 0; c < 6; c++)
			{
				const int index = c;
				if (index < (int)m_Order.size() && ActorAlive(m_Order[index]))
				{
					const int who = m_Order[index];
					const BattleLabel name = IsHero(who) ? GetHero(who).NameLabel : GetEnemy(m_Enemies[EnemyIndex(who)].Kind).NameLabel;
					SetSprite(scene, m_OrderChips[c], (uint32_t)name);
					const float strength = index == m_Turn ? 1.0f : (index < m_Turn ? 0.35f : 0.75f);
					const glm::vec3 tint = IsHero(who) ? glm::vec3(1.0f) : glm::vec3(1.0f, 0.7f, 0.65f);
					SetColor(scene, m_OrderChips[c], glm::vec4(tint * strength, 1.0f));
				}
				else
				{
					SetColor(scene, m_OrderChips[c], glm::vec4(0.0f));
				}
			}

			PresentSpriteEffect(scene, m_SlashEffect);
			PresentSpriteEffect(scene, m_HealEffect);

			// The message banner, damage numbers and the fade.
			SetSprite(scene, m_Message, (uint32_t)m_MessageLabel);
			const glm::vec3 messageTint = m_MessageLabel == BattleLabel::Break ? glm::vec3(1.0f, 0.45f, 0.3f)
				: m_MessageLabel == BattleLabel::Weak ? glm::vec3(1.0f, 0.75f, 0.3f) : glm::vec3(1.0f);
			const bool ending = m_Phase == Phase::Victory || m_Phase == Phase::Defeat || m_Phase == Phase::Fleeing || m_Phase == Phase::Done;
			SetColor(scene, m_Message, glm::vec4(messageTint, m_MessageTime > 0.0f || ending ? 1.0f : 0.0f));

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
		}

		Phase m_Phase = Phase::Intro;
		float m_PhaseTime = 0.0f;
		float m_Time = 0.0f;
		float m_DeltaTime = 0.0f;
		int m_StrikesPlayed = 0;   // extra swings of a boosted attack already started
		SpriteEffect m_SlashEffect;
		SpriteEffect m_HealEffect;

		// Timing that matches the battle sheets (scripts/shrine_glade/battle.py TIMING): the Attack clip's
		// contact frame starts after the anticipation (0.10 + 0.12) and the first two swing frames (0.07 + 0.06);
		// Strike is the swing alone. Spells peak on the Cast / Heal clip's third frame.
		static constexpr float FirstHitTime = 0.35f;
		static constexpr float StrikeContact = 0.13f;
		static constexpr float HitSpacing = 0.3f;
		static constexpr float SpellPeakTime = 0.38f;
		static constexpr float SlashEffectTime = 0.42f;
		static constexpr float HealEffectTime = 0.66f;
		int m_Round = 0;
		int m_Turn = -1;
		int m_Target = 0;   // the last target, kept for the next hero
		int m_Potions = 3;
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

		entt::entity m_Commands[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_CommandKeys[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_OrderChips[6] = { entt::null, entt::null, entt::null, entt::null, entt::null, entt::null };
		entt::entity m_Cursor = entt::null;
		entt::entity m_TurnMarker = entt::null;
		entt::entity m_BoostLabel = entt::null;
		entt::entity m_BoostDigit = entt::null;
		entt::entity m_Message = entt::null;
		entt::entity m_Overlay = entt::null;
	};
}
