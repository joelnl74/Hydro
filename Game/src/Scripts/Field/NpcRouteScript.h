// Game/src/Scripts/Field/NpcRouteScript.h
#pragma once

#include "Hydro.h"
#include "Rules/RouteRules.h"
#include "Scripts/Field/WaypointScript.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A townsperson who goes round a route: the Route entity's children are its waypoints (WaypointScript), walked in
	// the hierarchy's order by Mode (Loop, PingPong, Once, Restart; RouteRules). It walks straight from one to the
	// next (up a flight of stairs too: the waypoints carry the height), playing Walk<Facing>; at each it may stop and
	// do what the waypoint says (wait, face a way or an entity, play Action<Facing>). It keeps Clearance metres ahead
	// free: while the Player or another townsperson (a collider on the Npc layer) stands there it waits, facing
	// that way; after Patience seconds it turns back (PingPong) or walks on through for a moment.
	// Moves the transform directly (no physics body): its collider follows. Waypoints are read when they are
	// reached, so moving one while playing changes the route. Replaces NpcWanderScript (a two-waypoint route).
	HY_SCRIPT()
	class NpcRouteScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Required, Tooltip("The route: an entity whose children are the waypoints (WaypointScript), in order"))
		EntityRef Route;
		HY_PROPERTY(Tooltip("Loop (back to the first), PingPong (there and back), Once (then stays), Restart (reappears at the first)"))
		std::string Mode = "Loop";
		HY_PROPERTY(Range(0, 5), Tooltip("Metres per second (the player walks at about 3.5)"))
		float Speed = 1.1f;
		HY_PROPERTY(Range(0, 64), Tooltip("The waypoint it starts at (it is placed there), so a crowd on one route is spread out"))
		int StartAt = 0;
		HY_PROPERTY(Range(0, 60), Tooltip("Seconds before it first moves"))
		float StartDelay = 0.0f;
		HY_PROPERTY(Tooltip("The player: it waits while she stands in its way"))
		EntityRef Player;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres ahead it keeps free: it waits while the player or another townsperson stands there (0: walks through)"))
		float Clearance = 0.9f;
		HY_PROPERTY(Range(0, 30), Tooltip("Seconds it waits for its way to clear before it turns back (PingPong) or walks on through"))
		float Patience = 3.0f;

	protected:
		void OnCreate() override
		{
			m_Mode = RouteRules::ParseMode(Mode);
			m_Seed = 2654435761u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 3);
			m_Index = std::max(0, StartAt);
			m_Delay = StartDelay;
			m_Placed = false;
		}

		void OnUpdate(float deltaTime) override
		{
			if (Waypoints() == false)
			{
				return;
			}
			if (m_Placed == false)
			{
				// Start on its waypoint, then head for the next one.
				m_Index = std::min(m_Index, (int)m_Waypoints.size() - 1);
				SetWorldPosition(WaypointPosition(m_Index));
				m_Placed = true;
				Play("Idle", "Idle");
			}
			if (m_Delay > 0.0f)
			{
				m_Delay -= deltaTime;
				if (m_Delay <= 0.0f)
				{
					Arrive();
				}
				return;
			}
			if (m_Finished)
			{
				return;
			}
			if (m_Wait > 0.0f)
			{
				m_Wait -= deltaTime;
				if (m_Wait <= 0.0f)
				{
					Leave();
				}
				return;
			}

			const glm::vec3 position = GetWorldPosition();
			const glm::vec3 goal = WaypointPosition(m_Index);
			const glm::vec3 toGoal = goal - position;
			const float distance = glm::length(toGoal);
			const float step = Speed * deltaTime;
			if (distance <= std::max(step, 0.01f))
			{
				SetWorldPosition(goal);
				Arrive();
				return;
			}

			glm::vec3 forward{ toGoal.x, toGoal.y, 0.0f };
			const float flat = glm::length(forward);
			forward = flat > 1e-4f ? forward / flat : glm::vec3(0.0f);
			m_Facing = RouteRules::FacingOf(glm::vec2(forward), m_Facing);
			if (flat > 1e-4f && WaitWhileBlocked(position, forward, deltaTime))
			{
				return;
			}
			SetWorldPosition(position + toGoal / distance * step);
			Play("Walk", "Idle");
		}

	private:
		// The route's waypoints, looked up once (the route's children in order).
		bool Waypoints()
		{
			if (m_Looked)
			{
				return m_Waypoints.empty() == false;
			}
			m_Looked = true;
			Entity route = Route.Get();
			if ((bool)route == false)
			{
				HY_WARN("NpcRouteScript on '{}': no Route set", GetEntity().GetName());
				return false;
			}
			GetEntity().GetScene()->GetChildren(route, m_Waypoints);
			if (m_Waypoints.empty())
			{
				HY_WARN("NpcRouteScript on '{}': route '{}' has no waypoints (children)", GetEntity().GetName(), route.GetName());
			}
			return m_Waypoints.empty() == false;
		}

		glm::vec3 WaypointPosition(int index)
		{
			return GetEntity().GetScene()->GetWorldPosition(m_Waypoints[(size_t)index]);
		}

		// At waypoint m_Index: maybe stop there (its wait, facing and action), else go straight on.
		void Arrive()
		{
			const WaypointScript* waypoint = m_Waypoints[(size_t)m_Index].TryGetComponent<WaypointScript>();
			m_Wait = 0.0f;
			if (waypoint != nullptr && RouteRules::RollStop(waypoint->StopChance, Random01()))
			{
				m_Wait = RouteRules::RollWait(waypoint->WaitMin, waypoint->WaitMax, Random01());
			}
			if (m_Wait <= 0.0f)
			{
				Leave();
				return;
			}
			if (Entity look = waypoint->LookAt.Get())
			{
				const glm::vec3 toLook = GetEntity().GetScene()->GetWorldPosition(look) - GetWorldPosition();
				m_Facing = RouteRules::FacingOf(glm::vec2(toLook), m_Facing);
			}
			else if (waypoint->Facing.empty() == false)
			{
				m_Facing = waypoint->Facing;
			}
			Play(waypoint->Action.empty() ? "Idle" : waypoint->Action, "Idle");
		}

		// On to the next waypoint (or appear at the first: Restart; or stay: Once).
		void Leave()
		{
			const RouteRules::Step next = RouteRules::Next(m_Index, m_Direction, (int)m_Waypoints.size(), m_Mode);
			m_Index = next.Index;
			m_Direction = next.Direction;
			if (next.Finished)
			{
				m_Finished = true;
				Play("Idle", "Idle");
				return;
			}
			if (next.Teleport)
			{
				SetWorldPosition(WaypointPosition(m_Index));
				Arrive();
			}
		}

		// True (standing, facing the way) while the player or another townsperson is within Clearance ahead; after
		// Patience seconds of that it turns back (PingPong) or walks on through for a moment.
		bool WaitWhileBlocked(const glm::vec3& position, const glm::vec3& forward, float deltaTime)
		{
			if (m_PassThrough > 0.0f)
			{
				m_PassThrough -= deltaTime;
				return false;
			}
			if (Clearance <= 0.0f || IsBlocked(position, forward) == false)
			{
				m_Blocked = 0.0f;
				return false;
			}
			if (m_Blocked == 0.0f)
			{
				Play("Idle", "Idle");
			}
			m_Blocked += deltaTime;
			if (m_Blocked >= Patience)
			{
				m_Blocked = 0.0f;
				if (m_Mode == RouteRules::Mode::PingPong)
				{
					m_Direction = -m_Direction;
					m_Index = RouteRules::Next(m_Index, m_Direction, (int)m_Waypoints.size(), m_Mode).Index;
				}
				else
				{
					m_PassThrough = 1.5f;
				}
			}
			return true;
		}

		bool IsBlocked(const glm::vec3& position, const glm::vec3& forward)
		{
			Entity self = GetEntity();
			Scene& scene = *self.GetScene();
			if (Entity player = Player.Get())
			{
				if (RouteRules::IsInTheWay(position, forward, scene.GetWorldPosition(player), Clearance))
				{
					return true;
				}
			}
			m_Nearby.clear();
			const float radius = 0.5f * Clearance;
			Physics3D::OverlapSphere(scene, position + forward * (0.3f + radius) + glm::vec3(0.0f, 0.0f, 0.8f), radius,
				Physics3D::LayerBit(PhysicsLayer::Npc), m_Nearby);
			for (const Entity& other : m_Nearby)
			{
				if ((entt::entity)other != (entt::entity)self)
				{
					return true;
				}
			}
			return false;
		}

		// <prefix><Facing>, else <fallback><Facing>, else the sheet's plain Idle.
		void Play(const std::string& prefix, const std::string& fallback)
		{
			const std::string clip = prefix + m_Facing;
			if (clip == m_Clip)
			{
				return;
			}
			if (SpriteAnimator::Play(GetEntity(), clip) || SpriteAnimator::Play(GetEntity(), fallback + m_Facing)
				|| SpriteAnimator::Play(GetEntity(), "Idle"))
			{
				m_Clip = clip;
			}
		}

		// 0 to 1 from a xorshift: every townsperson keeps its own rhythm, the same every run.
		float Random01()
		{
			m_Seed ^= m_Seed << 13;
			m_Seed ^= m_Seed >> 17;
			m_Seed ^= m_Seed << 5;
			return (float)(m_Seed & 0xFFFFFF) / (float)0x1000000;
		}

		std::vector<Entity> m_Waypoints;
		std::vector<Entity> m_Nearby;
		RouteRules::Mode m_Mode = RouteRules::Mode::Loop;
		std::string m_Facing = "Down";
		std::string m_Clip;
		int m_Index = 0;
		int m_Direction = 1;
		float m_Wait = 0.0f;
		float m_Delay = 0.0f;        // StartDelay left: it stands on its first waypoint
		float m_Blocked = 0.0f;      // seconds waiting for the way to clear
		float m_PassThrough = 0.0f;  // seconds left walking on regardless
		uint32_t m_Seed = 1;
		bool m_Looked = false;
		bool m_Placed = false;
		bool m_Finished = false;
	};
}
