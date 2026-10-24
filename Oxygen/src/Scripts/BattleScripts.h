#pragma once
#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

// Native scripts of the battle prototype: a slime that wanders and chases the player in the village
// (Assets/Scenes/StatueVillage.hyscene, the Slime Field), and the turn-based battle scene it starts
// (Assets/Scenes/Battle.hyscene). The village scene is pushed down on the scene stack while the battle runs
// (SceneTransition) and carries on when it ends. There is no text rendering yet: the HP bars are coloured
// quads, the rest goes to the Console window.
//
// Battle keys: 1 attack, 2 defend (the next hit does half), 3 heal (3 potions), 4 run (50 %).
namespace Hydro
{
	// What the village and the battle scene tell each other (one game at a time, like VillageState).
	struct BattleState
	{
		enum class Result : uint8_t { None, Won, Lost, Fled };

		static constexpr int PlayerMaxHP = 40;
		static constexpr int EnemyMaxHP = 30;

		static inline int PlayerHP = PlayerMaxHP; // kept between battles, restored when the player loses
		static inline bool Active = false;        // a battle was started and has not been settled yet
		static inline uint64_t EnemyId = 0;       // UUID of the village slime that started it
		static inline Result Outcome = Result::None; // set by the battle for that slime to read when it is back

		// A new game: forget what the previous run left behind.
		static void Reset()
		{
			PlayerHP = PlayerMaxHP;
			Active = false;
			EnemyId = 0;
			Outcome = Result::None;
		}
	};

	// A slime for the village: wanders around where it was placed, runs after the entity tagged "Player" when
	// it comes close, gives up when the player gets far away, and starts the battle when it touches the player
	// (OnCollisionEnter: both need a body). Moves by velocity, so trees and fences stop it. After a battle the
	// player did not win it goes home and ignores the player for a few seconds; after a win it disappears.
	// Put it on a Dynamic Rigidbody2D (GravityScale 0, high LinearDamping, FixedRotation) with a Box collider.
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
			BattleState::Reset();
		}

		void OnUpdate(float deltaTime) override
		{
			Entity self = GetEntity();
			Scene* scene = self.GetScene();

			// The battle this slime started is over: the scene below carries on and reads the result.
			if (BattleState::Active && BattleState::EnemyId == (uint64_t)self.GetUUID() && BattleState::Outcome != BattleState::Result::None)
			{
				const BattleState::Result outcome = BattleState::Outcome;
				BattleState::Active = false;
				BattleState::Outcome = BattleState::Result::None;
				BattleState::EnemyId = 0;
				if (outcome == BattleState::Result::Won)
				{
					HY_INFO("Slime: defeated, it is gone");
					scene->DestroyEntity(self);
					return;
				}

				m_Cooldown = 4.0f;
				m_State = State::Return;
			}

			auto& transform = GetComponent<TransformComponent>();
			const glm::vec2 position{ transform.Translation.x, transform.Translation.y };
			m_Cooldown = std::max(0.0f, m_Cooldown - deltaTime);

			// Where the player is.
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
			}

			const float homeDistance = glm::length(m_Home - position);
			const bool canChase = m_Cooldown <= 0.0f && BattleState::Active == false;
			if (canChase && m_State != State::Chase && playerDistance < AggroRange)
			{
				m_State = State::Chase;
				HY_INFO("Slime: spotted the player");
			}
			else if (m_State == State::Chase && (canChase == false || playerDistance > LeashRange || homeDistance > MaxFromHome))
			{
				m_State = State::Return;
			}

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

					velocity = glm::normalize(toTarget) * WanderSpeed;
					break;
				}
				case State::Chase:
				{
					const glm::vec2 toPlayer = m_PlayerPosition - position;
					if (glm::length(toPlayer) > 0.01f)
					{
						velocity = glm::normalize(toPlayer) * ChaseSpeed;
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

					velocity = glm::normalize(toHome) * ReturnSpeed;
					break;
				}
			}

			if (Physics2D::SetLinearVelocity(self, velocity) == false)
			{
				transform.Translation.x += velocity.x * deltaTime;
				transform.Translation.y += velocity.y * deltaTime;
			}

			if (self.HasComponent<SpriteAnimatorComponent>())
			{
				GetComponent<SpriteAnimatorComponent>().Speed = animationSpeed;
			}
		}

		void OnCollisionEnter(Entity other) override
		{
			if (other.HasComponent<TagComponent>() == false || other.GetComponent<TagComponent>().Tag != "Player")
			{
				return;
			}

			if (m_Cooldown > 0.0f || BattleState::Active)
			{
				return;
			}

			// The village is paused under the battle: this slime remembers it started it.
			Entity self = GetEntity();
			BattleState::Active = true;
			BattleState::EnemyId = (uint64_t)self.GetUUID();
			BattleState::Outcome = BattleState::Result::None;
			Physics2D::SetLinearVelocity(self, glm::vec2(0.0f));
			HY_INFO("Slime: touched the player, battle starts (player HP {} of {})", BattleState::PlayerHP, BattleState::PlayerMaxHP);
			SceneTransition::Push("Battle");
		}

	private:
		enum class State : uint8_t { Wander, Chase, Return };

		static constexpr float AggroRange = 5.5f;   // the player this close wakes it up
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

		// 0 to 1 from a xorshift: the same slimes behave the same way every run, each differently.
		float Random01()
		{
			m_Seed ^= m_Seed << 13;
			m_Seed ^= m_Seed >> 17;
			m_Seed ^= m_Seed << 5;
			return (float)(m_Seed & 0xFFFFFF) / (float)0x1000000;
		}

		State m_State = State::Wander;
		entt::entity m_Player = entt::null;
		glm::vec2 m_Home{ 0.0f };
		glm::vec2 m_Target{ 0.0f };
		glm::vec2 m_PlayerPosition{ 0.0f };
		float m_Wait = 1.0f;
		float m_WalkTime = 0.0f;
		float m_Cooldown = 0.0f; // seconds the slime ignores the player (after a battle)
		uint32_t m_Seed = 1;
	};

	// The turn-based battle: put it on one entity of Battle.hyscene (the scene has a "Battle Director"). It finds
	// the scene's entities by tag: "Battle Player", "Battle Enemy", "Player HP Back/Fill", "Enemy HP Back/Fill",
	// "Turn Marker", "Command Attack/Defend/Heal/Run", "Player Shield" and "Battle Overlay". Each turn the
	// player presses 1 to 4, then the enemy answers. A turn takes about a second to play. When it ends the
	// result goes to BattleState::Outcome and the scene asks to be popped (SceneTransition::Pop), which
	// puts the village back.
	class BattleScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene* scene = GetEntity().GetScene();
			m_Player = Find(*scene, "Battle Player");
			m_Enemy = Find(*scene, "Battle Enemy");
			m_PlayerBack = Find(*scene, "Player HP Back");
			m_PlayerFill = Find(*scene, "Player HP Fill");
			m_EnemyBack = Find(*scene, "Enemy HP Back");
			m_EnemyFill = Find(*scene, "Enemy HP Fill");
			m_Marker = Find(*scene, "Turn Marker");
			m_Shield = Find(*scene, "Player Shield");
			m_Overlay = Find(*scene, "Battle Overlay");
			m_Commands[0] = Find(*scene, "Command Attack");
			m_Commands[1] = Find(*scene, "Command Defend");
			m_Commands[2] = Find(*scene, "Command Heal");
			m_Commands[3] = Find(*scene, "Command Run");

			m_PlayerBase = Position(*scene, m_Player);
			m_EnemyBase = Position(*scene, m_Enemy);
			m_EnemyHP = BattleState::EnemyMaxHP;

			// Keys held when the battle opens (the player was still walking) do not count.
			for (bool& previous : m_Previous)
			{
				previous = true;
			}

			SetPhase(Phase::Intro);
			HY_INFO("Battle: a slime blocks the way! (1 attack, 2 defend, 3 heal, 4 run)");
			ReportStatus();
		}

		void OnUpdate(float deltaTime) override
		{
			Scene* scene = GetEntity().GetScene();
			m_Time += deltaTime;
			m_PhaseTime += deltaTime;
			m_PlayerFlash = std::max(0.0f, m_PlayerFlash - deltaTime);
			m_EnemyFlash = std::max(0.0f, m_EnemyFlash - deltaTime);

			glm::vec2 playerOffset{ 0.0f };
			glm::vec2 enemyOffset{ 0.0f };
			float enemyAlpha = 1.0f;
			float overlayAlpha = 0.0f;

			switch (m_Phase)
			{
				case Phase::Intro:
				{
					// The slime drops in from above.
					const float t = std::min(1.0f, m_PhaseTime / 0.9f);
					enemyOffset.y = (1.0f - t) * (1.0f - t) * 12.0f;
					if (m_PhaseTime >= 1.2f)
					{
						SetPhase(Phase::PlayerTurn);
					}
					break;
				}
				case Phase::PlayerTurn:
				{
					const bool attack = Pressed(0, KeyCode::D1);
					const bool defend = Pressed(1, KeyCode::D2);
					const bool heal = Pressed(2, KeyCode::D3);
					const bool run = Pressed(3, KeyCode::D4);
					if (attack)
					{
						m_Applied = false;
						SetPhase(Phase::PlayerAttack);
					}
					else if (defend)
					{
						m_Defending = true;
						HY_INFO("Battle: you brace yourself, the next hit does half");
						SetPhase(Phase::PlayerDefend);
					}
					else if (heal)
					{
						if (m_Potions <= 0)
						{
							HY_INFO("Battle: no potions left");
						}
						else
						{
							m_Potions--;
							const int before = BattleState::PlayerHP;
							BattleState::PlayerHP = std::min(BattleState::PlayerMaxHP, BattleState::PlayerHP + HealAmount);
							HY_INFO("Battle: you drink a potion, +{} HP ({} left)", BattleState::PlayerHP - before, m_Potions);
							ReportStatus();
							SetPhase(Phase::PlayerHeal);
						}
					}
					else if (run)
					{
						if (Random01() < 0.5f)
						{
							HY_INFO("Battle: you got away");
							BattleState::Outcome = BattleState::Result::Fled;
							SetPhase(Phase::Fleeing);
						}
						else
						{
							HY_INFO("Battle: you could not escape");
							SetPhase(Phase::PlayerDefend);
						}
					}
					break;
				}
				case Phase::PlayerAttack:
				{
					// Runs at the slime and back; the hit lands halfway.
					const float t = std::min(1.0f, m_PhaseTime / 0.7f);
					const float reach = std::sin(t * 3.1415927f);
					playerOffset = glm::vec2(m_EnemyBase.x - m_PlayerBase.x - 4.5f, m_EnemyBase.y - m_PlayerBase.y) * reach;
					if (m_Applied == false && t >= 0.5f)
					{
						m_Applied = true;
						const bool critical = Random01() < 0.125f;
						int damage = 7 + (int)(Random01() * 6.0f);
						damage = critical ? damage * 2 : damage;
						m_EnemyHP = std::max(0, m_EnemyHP - damage);
						m_EnemyFlash = 0.35f;
						HY_INFO("Battle: you hit the slime for {}{}", damage, critical ? " (critical!)" : "");
						ReportStatus();
					}
					if (m_PhaseTime >= 0.9f)
					{
						SetPhase(m_EnemyHP <= 0 ? Phase::Victory : Phase::EnemyAttack);
					}
					break;
				}
				case Phase::PlayerDefend:
				case Phase::PlayerHeal:
				{
					if (m_PhaseTime >= 0.6f)
					{
						SetPhase(Phase::EnemyAttack);
					}
					break;
				}
				case Phase::EnemyAttack:
				{
					if (m_PhaseTime < 0.5f)
					{
						break; // the slime thinks about it
					}

					const float t = std::min(1.0f, (m_PhaseTime - 0.5f) / 0.7f);
					const float reach = std::sin(t * 3.1415927f);
					enemyOffset = glm::vec2(m_PlayerBase.x - m_EnemyBase.x + 4.0f, m_PlayerBase.y - m_EnemyBase.y) * reach;
					if (m_Applied == false && t >= 0.5f)
					{
						m_Applied = true;
						int damage = 4 + (int)(Random01() * 5.0f);
						if (m_Defending)
						{
							damage = std::max(1, damage / 2);
							m_Defending = false;
						}
						BattleState::PlayerHP = std::max(0, BattleState::PlayerHP - damage);
						m_PlayerFlash = 0.35f;
						HY_INFO("Battle: the slime hits you for {}", damage);
						ReportStatus();
					}
					if (m_PhaseTime >= 1.5f)
					{
						SetPhase(BattleState::PlayerHP <= 0 ? Phase::Defeat : Phase::PlayerTurn);
					}
					break;
				}
				case Phase::Victory:
				{
					enemyAlpha = 1.0f - std::min(1.0f, m_PhaseTime / 0.7f);
					overlayAlpha = std::clamp((m_PhaseTime - 0.8f) / 0.6f, 0.0f, 1.0f);
					if (m_PhaseTime >= 1.6f)
					{
						Finish(BattleState::Result::Won);
					}
					break;
				}
				case Phase::Defeat:
				{
					overlayAlpha = std::clamp(m_PhaseTime / 0.8f, 0.0f, 1.0f);
					if (m_PhaseTime >= 1.5f)
					{
						// Down: the village sees the player back on their feet.
						BattleState::PlayerHP = BattleState::PlayerMaxHP;
						Finish(BattleState::Result::Lost);
					}
					break;
				}
				case Phase::Fleeing:
				{
					overlayAlpha = std::clamp(m_PhaseTime / 0.5f, 0.0f, 1.0f);
					if (m_PhaseTime >= 0.7f)
					{
						Finish(BattleState::Result::Fled);
					}
					break;
				}
				case Phase::Done:
					break;
			}

			Apply(*scene, playerOffset, enemyOffset, enemyAlpha, overlayAlpha);
		}

	private:
		enum class Phase : uint8_t
		{
			Intro,
			PlayerTurn,
			PlayerAttack,
			PlayerDefend,
			PlayerHeal,
			EnemyAttack,
			Victory,
			Defeat,
			Fleeing,
			Done // asked for the pop; waiting for the program to do it
		};

		static constexpr int HealAmount = 14;

		void SetPhase(Phase phase)
		{
			m_Phase = phase;
			m_PhaseTime = 0.0f;
			m_Applied = false;
			if (phase == Phase::Victory)
			{
				HY_INFO("Battle: the slime is beaten, you win!");
			}
			else if (phase == Phase::Defeat)
			{
				HY_INFO("Battle: you are out of strength...");
			}
			else if (phase == Phase::PlayerTurn)
			{
				HY_INFO("Battle: your turn (1 attack, 2 defend, 3 heal, 4 run)");
			}
		}

		void Finish(BattleState::Result outcome)
		{
			BattleState::Outcome = outcome;
			m_Phase = Phase::Done;
			SceneTransition::Pop();
		}

		bool Pressed(int index, KeyCode key)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && m_Previous[index] == false;
			m_Previous[index] = down;
			return pressed;
		}

		float Random01() { return (float)(m_Random() & 0xFFFFFF) / (float)0x1000000; }

		void ReportStatus() const
		{
			HY_INFO("Battle: you {}/{} HP | slime {}/{} HP | potions {}", BattleState::PlayerHP, BattleState::PlayerMaxHP,
				m_EnemyHP, BattleState::EnemyMaxHP, m_Potions);
		}

		static entt::entity Find(Scene& scene, const char* tag)
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

		// A bar made of two quads: the back one is as wide as a full bar, the fill sticks to its left end.
		static void SetBar(Scene& scene, entt::entity back, entt::entity fill, float fraction)
		{
			if (Alive(scene, back) == false || Alive(scene, fill) == false)
			{
				return;
			}

			const TransformComponent& backTransform = scene.m_Registry.get<TransformComponent>(back);
			TransformComponent& fillTransform = scene.m_Registry.get<TransformComponent>(fill);
			const float width = backTransform.Scale.x - 0.3f;
			const float length = std::max(0.0f, width * std::clamp(fraction, 0.0f, 1.0f));
			fillTransform.Scale.x = length;
			fillTransform.Scale.y = backTransform.Scale.y - 0.3f;
			fillTransform.Translation.x = backTransform.Translation.x - 0.5f * width + 0.5f * length;
			fillTransform.Translation.y = backTransform.Translation.y;

			const glm::vec4 color = fraction > 0.5f ? glm::vec4(0.25f, 0.85f, 0.35f, 1.0f)
				: fraction > 0.25f ? glm::vec4(0.95f, 0.8f, 0.2f, 1.0f)
				: glm::vec4(0.9f, 0.2f, 0.2f, 1.0f);
			SetColor(scene, fill, color);
		}

		void Apply(Scene& scene, const glm::vec2& playerOffset, const glm::vec2& enemyOffset, float enemyAlpha, float overlayAlpha)
		{
			// Hit shake while flashing.
			const float playerShake = m_PlayerFlash > 0.0f ? std::sin(m_Time * 70.0f) * 0.25f : 0.0f;
			const float enemyShake = m_EnemyFlash > 0.0f ? std::sin(m_Time * 70.0f) * 0.25f : 0.0f;
			if (Alive(scene, m_Player))
			{
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(m_Player);
				transform.Translation.x = m_PlayerBase.x + playerOffset.x + playerShake;
				transform.Translation.y = m_PlayerBase.y + playerOffset.y;
				const float heat = std::clamp(m_PlayerFlash / 0.35f, 0.0f, 1.0f);
				SetColor(scene, m_Player, glm::vec4(1.0f, 1.0f - 0.65f * heat, 1.0f - 0.65f * heat, 1.0f));
			}
			if (Alive(scene, m_Enemy))
			{
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(m_Enemy);
				transform.Translation.x = m_EnemyBase.x + enemyOffset.x + enemyShake;
				transform.Translation.y = m_EnemyBase.y + enemyOffset.y;
				const float heat = std::clamp(m_EnemyFlash / 0.35f, 0.0f, 1.0f);
				SetColor(scene, m_Enemy, glm::vec4(1.0f, 1.0f - 0.65f * heat, 1.0f - 0.65f * heat, enemyAlpha));
			}

			SetBar(scene, m_PlayerBack, m_PlayerFill, (float)BattleState::PlayerHP / (float)BattleState::PlayerMaxHP);
			SetBar(scene, m_EnemyBack, m_EnemyFill, (float)m_EnemyHP / (float)BattleState::EnemyMaxHP);

			// The marker bobs over whoever acts.
			if (Alive(scene, m_Marker))
			{
				const bool playerActs = m_Phase == Phase::PlayerTurn;
				const bool enemyActs = m_Phase == Phase::EnemyAttack;
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(m_Marker);
				const float bob = std::sin(m_Time * 6.0f) * 0.25f;
				// Above the HP bar of the one who acts (the characters are 7 and 5 units tall).
				transform.Translation.x = playerActs ? m_PlayerBase.x : m_EnemyBase.x;
				transform.Translation.y = (playerActs ? m_PlayerBase.y + 9.3f : m_EnemyBase.y + 6.9f) + bob;
				SetColor(scene, m_Marker, glm::vec4(1.0f, 0.88f, 0.2f, (playerActs || enemyActs) ? 1.0f : 0.0f));
			}

			// The commands light up while it is the player's turn; a heal with no potions left stays dim.
			const bool choosing = m_Phase == Phase::PlayerTurn;
			const glm::vec4 colors[4] = {
				{ 0.9f, 0.25f, 0.25f, 1.0f }, { 0.25f, 0.45f, 0.95f, 1.0f }, { 0.3f, 0.85f, 0.4f, 1.0f }, { 0.85f, 0.85f, 0.85f, 1.0f }
			};
			for (int i = 0; i < 4; i++)
			{
				float strength = choosing ? 1.0f : 0.25f;
				if (i == 2 && m_Potions <= 0)
				{
					strength = 0.25f;
				}
				SetColor(scene, m_Commands[i], glm::vec4(colors[i].r, colors[i].g, colors[i].b, strength));
			}

			SetColor(scene, m_Shield, glm::vec4(0.35f, 0.6f, 1.0f, m_Defending ? 0.45f : 0.0f));

			const bool won = m_Phase == Phase::Victory;
			SetColor(scene, m_Overlay, won ? glm::vec4(1.0f, 1.0f, 0.8f, overlayAlpha)
				: m_Phase == Phase::Defeat ? glm::vec4(0.55f, 0.0f, 0.0f, overlayAlpha)
				: glm::vec4(0.0f, 0.0f, 0.0f, overlayAlpha));
		}

		Phase m_Phase = Phase::Intro;
		float m_PhaseTime = 0.0f;
		float m_Time = 0.0f;
		bool m_Applied = false;     // the current attack's damage was dealt
		bool m_Defending = false;
		int m_Potions = 3;
		int m_EnemyHP = BattleState::EnemyMaxHP;
		float m_PlayerFlash = 0.0f;
		float m_EnemyFlash = 0.0f;
		bool m_Previous[4] = {};
		std::mt19937 m_Random{ std::random_device{}() };

		entt::entity m_Player = entt::null;
		entt::entity m_Enemy = entt::null;
		entt::entity m_PlayerBack = entt::null;
		entt::entity m_PlayerFill = entt::null;
		entt::entity m_EnemyBack = entt::null;
		entt::entity m_EnemyFill = entt::null;
		entt::entity m_Marker = entt::null;
		entt::entity m_Shield = entt::null;
		entt::entity m_Overlay = entt::null;
		entt::entity m_Commands[4] = { entt::null, entt::null, entt::null, entt::null };
		glm::vec2 m_PlayerBase{ 0.0f };
		glm::vec2 m_EnemyBase{ 0.0f };
	};

	inline void RegisterBattleScripts()
	{
		// The names are what scenes save: keep them stable.
		ScriptRegistry::Register<EnemyChaseScript>("EnemyChase");
		ScriptRegistry::Register<BattleScript>("Battle");
	}
}
