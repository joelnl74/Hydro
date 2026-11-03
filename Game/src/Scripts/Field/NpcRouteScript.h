#pragma once

#include "Hydro.h"
#include "Rules/RouteRules.h"
#include "Scripts/Field/WaypointComponent.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	// A townsperson who walks a route of waypoints by Mode, doing what each WaypointComponent says, and waits while the way
	// ahead is blocked. Moves the transform directly; waypoints are read when reached, so they can be moved while playing.
	HY_SCRIPT()
	class NpcRouteScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Tooltip("The waypoints in the order they are walked (any entities, in any order in the hierarchy); empty: the Route's children"))
		std::vector<EntityRef> Waypoints;
		HY_PROPERTY(Tooltip("Used when Waypoints is empty: an entity whose children are the waypoints, in the hierarchy's order"))
		EntityRef Route;
		HY_PROPERTY(Tooltip("Loop (back to the first), PingPong (there and back), Once (then stays), Restart (reappears at the first)"))
		RouteMode Mode = RouteMode::Loop;
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
		// Bright while the townsperson, its route or a waypoint is selected, with each waypoint's number and what it does there.
		void OnDrawGizmos(Gizmos& gizmos) override
		{
			std::vector<Entity> points;
			GatherWaypoints(points, false);
			if (points.empty())
			{
				return;
			}
			Scene& scene = *GetEntity().GetScene();
			bool selected = gizmos.IsSelected() || gizmos.IsSelected(Route.Get());
			for (const Entity& point : points)
			{
				selected = selected || gizmos.IsSelected(point);
			}
			const glm::vec4 color = selected ? Gizmos::Highlight : Gizmos::Normal;
			const glm::vec3 lift(0.0f, 0.0f, 0.05f);

			const size_t count = points.size();
			const bool closes = Mode == RouteMode::Loop && count > 2;
			for (size_t i = 0; i < count; i++)
			{
				const glm::vec3 here = scene.GetWorldPosition(points[i]) + lift;
				gizmos.Cross(here, 0.35f, color);
				if (i + 1 < count || closes)
				{
					const glm::vec3 next = scene.GetWorldPosition(points[(i + 1) % count]) + lift;
					if (Mode == RouteMode::PingPong)
					{
						gizmos.Line(here, next, color);
					}
					else
					{
						gizmos.Arrow(here, next, color);
					}
				}
				if (selected)
				{
					std::string text = std::to_string(i + 1);
					if (const WaypointComponent* waypoint = points[i].TryGetComponent<WaypointComponent>())
					{
						if (waypoint->WaitMax > 0.0f)
						{
							char wait[48];
							std::snprintf(wait, sizeof(wait), "  wait %g-%g s", waypoint->WaitMin,
								std::max(waypoint->WaitMin, waypoint->WaitMax));
							text += wait;
							if (waypoint->StopChance < 1.0f)
							{
								std::snprintf(wait, sizeof(wait), " (%d%%)", (int)(waypoint->StopChance * 100.0f + 0.5f));
								text += wait;
							}
						}
						if (waypoint->Action.empty() == false)
						{
							text += ", " + waypoint->Action;
						}
					}
					gizmos.Label(here + glm::vec3(0.0f, 0.0f, 0.5f), text, color);
				}
			}
			// Where it starts: a line from the townsperson to its first waypoint.
			if (selected)
			{
				const size_t start = std::min((size_t)std::max(0, StartAt), count - 1);
				gizmos.Line(GetWorldPosition() + lift, scene.GetWorldPosition(points[start]) + lift,
					glm::vec4(color.r, color.g, color.b, 0.4f), 1.0f);
			}
		}

		void OnCreate() override
		{
			m_Mode = Mode;
			m_Seed = 2654435761u ^ (uint32_t)((uint64_t)GetEntity().GetUUID() >> 3);
			m_Index = std::max(0, StartAt);
			m_Delay = StartDelay;
			m_Placed = false;
		}

		void OnUpdate(float deltaTime) override
		{
			if (ResolveWaypoints() == false)
			{
				return;
			}
			if (m_Placed == false)
			{
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
		// Entries whose entity is gone are skipped. `warn`: say what is missing (once, when it starts walking; not for gizmos).
		void GatherWaypoints(std::vector<Entity>& waypoints, bool warn)
		{
			waypoints.clear();
			if (Waypoints.empty() == false)
			{
				for (const EntityRef& waypoint : Waypoints)
				{
					if (Entity entity = waypoint.Get())
					{
						waypoints.push_back(entity);
					}
					else if (warn)
					{
						HY_WARN("NpcRouteScript on '{}': a waypoint in its list is missing (skipped)", GetEntity().GetName());
					}
				}
				return;
			}
			Entity route = Route.Get();
			if ((bool)route == false)
			{
				if (warn)
				{
					HY_WARN("NpcRouteScript on '{}': no Waypoints and no Route set", GetEntity().GetName());
				}
				return;
			}
			GetEntity().GetScene()->GetChildren(route, waypoints);
			if (waypoints.empty() && warn)
			{
				HY_WARN("NpcRouteScript on '{}': route '{}' has no waypoints (children)", GetEntity().GetName(), route.GetName());
			}
		}

		// Looked up once, when it first moves.
		bool ResolveWaypoints()
		{
			if (m_Looked == false)
			{
				m_Looked = true;
				GatherWaypoints(m_Waypoints, true);
			}
			return m_Waypoints.empty() == false;
		}

		glm::vec3 WaypointPosition(int index)
		{
			return GetEntity().GetScene()->GetWorldPosition(m_Waypoints[(size_t)index]);
		}

		// Maybe stop at waypoint m_Index (its wait, facing and action), else go straight on.
		void Arrive()
		{
			const WaypointComponent* waypoint = m_Waypoints[(size_t)m_Index].TryGetComponent<WaypointComponent>();
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

		// Appears at the first waypoint for Restart, stays for Once.
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

		// True while someone is within Clearance ahead; after Patience seconds it turns back (PingPong) or walks on through.
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
