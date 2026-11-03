#pragma once

#include "Hydro.h"
#include "State/BattleState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A field enemy: wanders near home, chases the player when close and starts a battle on touch. Its Enemy and Encounter
	// rows say what it is and who fights; a Dormant one waits hidden until a quest wakes it.
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
		HY_PROPERTY(DataRow(EnemyDefinition), Tooltip("What it is; unset: a tag starting with \"Wisp\" is a Wisp, anything else a Slime"))
		DataRowRef Enemy;
		HY_PROPERTY(DataRow(EncounterDefinition), Tooltip("Who fights when it touches the player; unset: it and up to two random slimes or wisps"))
		DataRowRef Encounter;

		// Nothing when it is awake.
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
			if (Enemy.IsSet())
			{
				m_Row = Enemy.Row;
			}
			else if (GetEntity().HasComponent<TagComponent>() && GetComponent<TagComponent>().Tag.rfind("Wisp", 0) == 0)
			{
				m_Row = "Wisp";
			}

			// Dormant: invisible and parked far below the map, so its collider blocks nobody.
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
			if (BattleState::Get().Active && BattleState::Get().EnemyId == (uint64_t)self.GetUUID()
				&& BattleState::Get().Outcome != BattleState::Result::None)
			{
				const BattleState::Result outcome = BattleState::Get().Outcome;
				BattleState::Get().Active = false;
				BattleState::Get().Outcome = BattleState::Result::None;
				BattleState::Get().EnemyId = 0;
				if (outcome == BattleState::Result::Won)
				{
					HY_INFO("{}: defeated, it is gone", GetEnemy(m_Row).Name);
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

				// A player without a physics body never collides: touching is being this close on the same level.
				if (scene->m_Registry.has<Rigidbody2DComponent>(m_Player) == false && playerDistance < TouchRange
					&& std::abs(playerPosition.z - world.z) < 0.5f)
				{
					StartBattle();
					return;
				}
			}

			const float homeDistance = glm::length(m_Home - position);
			const bool canChase = m_Cooldown <= 0.0f && BattleState::Get().Active == false;
			if (canChase && m_State != State::Chase && playerDistance < AggroRange)
			{
				m_State = State::Chase;
				HY_INFO("{}: spotted the player", GetEnemy(m_Row).Name);
			}
			else if (m_State == State::Chase && (canChase == false || playerDistance > LeashRange || homeDistance > MaxFromHome))
			{
				m_State = State::Return;
			}

			const EnemyDefinition& definition = GetEnemy(m_Row);
			const float speedScale = definition.FieldSpeed;
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

			// Floating ones (wisps) sway sideways on top of where they are going.
			if (definition.FloatHeight > 0.0f)
			{
				velocity.x += std::sin(m_Time * 1.7f + (float)(m_Seed & 7)) * 0.35f;
			}

			if (Physics2D::SetLinearVelocity(self, velocity) == false)
			{
				SetWorldPosition(world + glm::vec3(velocity * deltaTime, 0.0f));
			}

			SpriteAnimator::SetSpeed(self, animationSpeed);

			if (self.HasComponent<Light2DComponent>())
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
		// Once per touch: not while cooling down after a battle or while one runs.
		void StartBattle()
		{
			if (m_Cooldown > 0.0f || BattleState::Get().Active)
			{
				return;
			}

			Entity self = GetEntity();
			BattleState::Get().Active = true;
			BattleState::Get().EnemyId = (uint64_t)self.GetUUID();
			BattleState::Get().Outcome = BattleState::Result::None;
			std::array<std::string, BattleState::MaxEnemies>& slots = BattleState::Get().Encounter;
			slots = { m_Row, std::string(), std::string() };
			int count = 1;
			const DataView<EncounterDefinition> encounter = Encounter.IsSet() ? Data::GetRow<EncounterDefinition>(Encounter)
				: DataView<EncounterDefinition>();
			BattleState::Get().EncounterLoot = encounter.IsValid() ? encounter->Loot : DataRowRef();
			if (encounter.IsValid() && encounter->Enemies.empty() == false)
			{
				count = 0;
				for (const DataRowRef& member : encounter->Enemies)
				{
					if (count < BattleState::MaxEnemies && member.IsSet())
					{
						slots[(size_t)count++] = member.Row;
					}
				}
			}
			else
			{
				// The default: up to two friends, wisps more often when it is one.
				const float roll = Random01();
				const int extra = roll < 0.3f ? 0 : (roll < 0.75f ? 1 : 2);
				for (int i = 1; i <= extra; i++)
				{
					slots[(size_t)i] = Random01() < (m_Row == "Wisp" ? 0.5f : 0.3f) ? "Wisp" : "Slime";
				}
				count = 1 + extra;
			}

			Physics2D::SetLinearVelocity(self, glm::vec2(0.0f));
			HY_INFO("{}: touched the player, battle starts ({} enemies)", GetEnemy(m_Row).Name, count);
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

		// The sprite's and its blob shadow's opacity.
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

		std::string m_Row = "Slime"; // its row of Enemies.hytable
		State m_State = State::Wander;
		entt::entity m_Player = entt::null;
		glm::vec2 m_Home{ 0.0f };
		glm::vec2 m_Target{ 0.0f };
		glm::vec2 m_PlayerPosition{ 0.0f };
		float m_Wait = 1.0f;
		float m_WalkTime = 0.0f;
		float m_Cooldown = 0.0f; // seconds the enemy ignores the player after a battle
		float m_Time = 0.0f;
		float m_LightBase = -1.0f;
		float m_ShadowStrength = -1.0f;
		float m_Appear = 1.0f;          // 0..1 while a woken enemy fades in
		glm::vec3 m_DormantPlace{ 0.0f }; // where it was placed (a dormant one waits below it)
		uint32_t m_Seed = 1;
	};
}
