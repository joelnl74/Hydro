#include "hypch.h"
#include "ParticleScene.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SystemContext.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Hydro
{
	// ---- Random numbers (xorshift32: small, fast, repeatable from a seed) ---------

	static uint32_t NextRandom(uint32_t& state)
	{
		uint32_t x = state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		state = x;
		return x;
	}

	// 0 to 1.
	static float Random01(uint32_t& state)
	{
		return (float)(NextRandom(state) >> 8) * (1.0f / 16777216.0f);
	}

	static float RandomIn(uint32_t& state, const FloatRange& range)
	{
		return range.Min + (range.Max - range.Min) * Random01(state);
	}

	// xorshift must not start at 0.
	static uint32_t MakeSeed(uint32_t seed)
	{
		uint32_t mixed = seed * 2654435761u + 0x9E3779B9u;
		return mixed == 0 ? 1u : mixed;
	}

	// ---- Buffers ------------------------------------------------------------------

	static void ResizeBuffer(ParticleScene::ParticleBuffer& buffer, uint32_t capacity)
	{
		buffer.Position.resize(capacity);
		buffer.Velocity.resize(capacity);
		buffer.Age.resize(capacity);
		buffer.Lifetime.resize(capacity);
		buffer.StartSize.resize(capacity);
		buffer.Rotation.resize(capacity);
		buffer.AngularVelocity.resize(capacity);
	}

	// Moves particle `from` into slot `to` (swap-remove).
	static void MoveParticle(ParticleScene::ParticleBuffer& buffer, uint32_t to, uint32_t from)
	{
		buffer.Position[to] = buffer.Position[from];
		buffer.Velocity[to] = buffer.Velocity[from];
		buffer.Age[to] = buffer.Age[from];
		buffer.Lifetime[to] = buffer.Lifetime[from];
		buffer.StartSize[to] = buffer.StartSize[from];
		buffer.Rotation[to] = buffer.Rotation[from];
		buffer.AngularVelocity[to] = buffer.AngularVelocity[from];
	}

	ParticleScene::EmitterRuntime& ParticleScene::GetOrCreate(entt::entity emitter)
	{
		auto found = m_Emitters.find(emitter);
		if (found != m_Emitters.end())
		{
			return found->second;
		}
		return m_Emitters[emitter];
	}

	void ParticleScene::Reserve(EmitterRuntime& runtime, uint32_t capacity)
	{
		ParticleBuffer& buffer = runtime.Particles;
		if (buffer.GetCapacity() >= capacity)
		{
			return;
		}

		// A first buffer: reuse a freed one that is big enough (the smallest such), else make one.
		if (buffer.GetCapacity() == 0 && buffer.Count == 0)
		{
			size_t best = m_FreeBuffers.size();
			for (size_t i = 0; i < m_FreeBuffers.size(); i++)
			{
				const uint32_t freeCapacity = m_FreeBuffers[i].GetCapacity();
				if (freeCapacity >= capacity && (best == m_FreeBuffers.size() || freeCapacity < m_FreeBuffers[best].GetCapacity()))
				{
					best = i;
				}
			}
			if (best < m_FreeBuffers.size())
			{
				buffer = std::move(m_FreeBuffers[best]);
				m_FreeBuffers[best] = std::move(m_FreeBuffers.back());
				m_FreeBuffers.pop_back();
				buffer.Count = 0;
				return;
			}
		}

		// Grows in place; resize keeps the live particles at the front.
		ResizeBuffer(buffer, capacity);
	}

	void ParticleScene::ReleaseBuffer(ParticleBuffer& buffer)
	{
		m_LiveParticles -= std::min(m_LiveParticles, buffer.Count);
		buffer.Count = 0;
		if (buffer.GetCapacity() > 0)
		{
			m_FreeBuffers.push_back(std::move(buffer));
			buffer = ParticleBuffer();
		}
	}

	// ---- Emission and simulation --------------------------------------------------

	uint32_t ParticleScene::Emit(EmitterRuntime& runtime, const ParticleEmitter2DComponent& emitter, const glm::vec2& origin, uint32_t count)
	{
		ParticleBuffer& buffer = runtime.Particles;
		const uint32_t limit = std::min(emitter.MaxParticles, buffer.GetCapacity());
		uint32_t emitted = 0;
		for (; emitted < count; emitted++)
		{
			if (buffer.Count >= limit || m_LiveParticles >= MaxLiveParticles)
			{
				break;
			}

			uint32_t& random = runtime.Random;
			glm::vec2 offset{ 0.0f };
			if (emitter.Shape == EmitterShape::Circle)
			{
				// sqrt spreads the points evenly over the disc instead of bunching them at the centre.
				const float angle = Random01(random) * glm::two_pi<float>();
				const float radius = emitter.ShapeSize.x * std::sqrt(Random01(random));
				offset = { std::cos(angle) * radius, std::sin(angle) * radius };
			}
			else if (emitter.Shape == EmitterShape::Box)
			{
				offset = { (Random01(random) - 0.5f) * emitter.ShapeSize.x, (Random01(random) - 0.5f) * emitter.ShapeSize.y };
			}

			const float angle = glm::radians(emitter.Direction + (Random01(random) * 2.0f - 1.0f) * emitter.Spread);
			const float speed = RandomIn(random, emitter.Speed);

			const uint32_t i = buffer.Count++;
			buffer.Position[i] = origin + offset;
			buffer.Velocity[i] = { std::cos(angle) * speed, std::sin(angle) * speed };
			buffer.Age[i] = 0.0f;
			buffer.Lifetime[i] = std::max(0.01f, RandomIn(random, emitter.Lifetime));
			buffer.StartSize[i] = std::max(0.0f, RandomIn(random, emitter.StartSize));
			buffer.Rotation[i] = glm::radians(RandomIn(random, emitter.StartRotation));
			buffer.AngularVelocity[i] = glm::radians(RandomIn(random, emitter.AngularVelocity));
			m_LiveParticles++;
		}

		if (emitted > 0)
		{
			runtime.HasEmitted = true;
		}
		return count - emitted;
	}

	void ParticleScene::Update(Scene& scene, const SystemContext& context)
	{
		const auto start = std::chrono::steady_clock::now();
		const float dt = context.DeltaTime;
		uint32_t dropped = 0;

		m_Finished.clear();
		auto view = scene.m_Registry.view<TransformComponent, ParticleEmitter2DComponent>();
		for (auto entity : view)
		{
			const auto [transform, emitter] = view.get<TransformComponent, ParticleEmitter2DComponent>(entity);
			EmitterRuntime& runtime = GetOrCreate(entity);

			if (runtime.Random == 0)
			{
				// Seed 0: a different pattern on every playback; otherwise the same every time.
				runtime.Random = MakeSeed(emitter.Seed != 0 ? emitter.Seed : (uint32_t)entt::to_integral(entity) ^ (++m_PlaybackCounter * 0x85EBCA6Bu));
			}
			if (runtime.Started == false)
			{
				runtime.Started = true;
				if (emitter.PlayOnStart)
				{
					runtime.Emitting = true;
					runtime.Time = 0.0f;
					runtime.PendingBurst += emitter.BurstCount;
				}
			}

			// The buffer follows MaxParticles (changed in the inspector during play: grows, or drops the extra).
			Reserve(runtime, emitter.MaxParticles);
			ParticleBuffer& buffer = runtime.Particles;
			if (buffer.Count > emitter.MaxParticles)
			{
				m_LiveParticles -= std::min(m_LiveParticles, buffer.Count - emitter.MaxParticles);
				buffer.Count = emitter.MaxParticles;
			}

			// Emission: the rate over time, and BurstCount at the start of every loop. A long frame can
			// cross several loops (Duration is at least 0.01 s and dt at most Time::MaxDeltaTime).
			if (runtime.LoopBurstPending)
			{
				runtime.LoopBurstPending = false;
				runtime.PendingBurst += emitter.BurstCount;
			}
			uint32_t toEmit = runtime.PendingBurst;
			runtime.PendingBurst = 0;
			if (runtime.Emitting)
			{
				runtime.EmitAccumulator += std::max(emitter.Rate, 0.0f) * dt;
				runtime.Time += dt;
				const float duration = std::max(emitter.Duration, 0.01f);
				while (runtime.Time >= duration)
				{
					if (emitter.Looping)
					{
						runtime.Time -= duration;
						toEmit += emitter.BurstCount;
					}
					else
					{
						runtime.Emitting = false;
						runtime.Time = duration;
						break;
					}
				}
				const float whole = std::floor(runtime.EmitAccumulator);
				runtime.EmitAccumulator -= whole;
				// Never more than the emitter can hold in one update (a huge Rate or a long hitch).
				toEmit += (uint32_t)std::min(whole, (float)ParticleEmitter2DComponent::MaxParticlesLimit);
			}
			else
			{
				runtime.EmitAccumulator = 0.0f;
			}

			// Simulate first, so this frame's new particles start at their origin.
			const glm::vec2 gravity = emitter.Gravity;
			const float damping = 1.0f / (1.0f + std::max(emitter.Drag, 0.0f) * dt);
			const float turbulence = std::max(emitter.Turbulence, 0.0f);
			uint32_t i = 0;
			while (i < buffer.Count)
			{
				buffer.Age[i] += dt;
				if (buffer.Age[i] >= buffer.Lifetime[i])
				{
					buffer.Count--;
					m_LiveParticles -= std::min(m_LiveParticles, 1u);
					if (i < buffer.Count)
					{
						MoveParticle(buffer, i, buffer.Count);
					}
					continue;
				}

				glm::vec2 acceleration = gravity;
				if (turbulence > 0.0f)
				{
					const float angle = Random01(runtime.Random) * glm::two_pi<float>();
					acceleration += glm::vec2(std::cos(angle), std::sin(angle)) * turbulence;
				}
				buffer.Velocity[i] = (buffer.Velocity[i] + acceleration * dt) * damping;
				buffer.Position[i] += buffer.Velocity[i] * dt;
				buffer.Rotation[i] += buffer.AngularVelocity[i] * dt;
				i++;
			}

			if (toEmit > 0)
			{
				dropped += Emit(runtime, emitter, glm::vec2(transform.Translation), toEmit);
			}

			if (emitter.DestroyWhenFinished && runtime.HasEmitted && runtime.Emitting == false && buffer.Count == 0 && runtime.PendingBurst == 0)
			{
				m_Finished.push_back(entity);
			}
		}

		// Structural changes after the view loop.
		for (entt::entity entity : m_Finished)
		{
			scene.DestroyEntity(Entity(entity, &scene));
		}

		// Drop entries of emitters that are gone (destroyed above or elsewhere, or the component removed).
		for (auto it = m_Emitters.begin(); it != m_Emitters.end();)
		{
			const bool exists = scene.m_Registry.valid(it->first) && scene.m_Registry.has<ParticleEmitter2DComponent>(it->first);
			if (exists)
			{
				++it;
				continue;
			}
			ReleaseBuffer(it->second.Particles);
			it = m_Emitters.erase(it);
		}

		m_Stats.Emitters = (uint32_t)m_Emitters.size();
		m_Stats.LiveParticles = m_LiveParticles;
		m_Stats.DroppedParticles = dropped;
		m_Stats.ReservedParticles = 0;
		for (const auto& [entity, runtime] : m_Emitters)
		{
			m_Stats.ReservedParticles += runtime.Particles.GetCapacity();
		}
		for (const ParticleBuffer& buffer : m_FreeBuffers)
		{
			m_Stats.ReservedParticles += buffer.GetCapacity();
		}
		m_Stats.UpdateMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
	}

	void ParticleScene::Clear()
	{
		for (auto& [entity, runtime] : m_Emitters)
		{
			ReleaseBuffer(runtime.Particles);
		}
		m_Emitters.clear();
		m_LiveParticles = 0;
		m_Stats = {};
	}

	// ---- Game control -------------------------------------------------------------

	void ParticleScene::Play(entt::entity emitter)
	{
		EmitterRuntime& runtime = GetOrCreate(emitter);
		runtime.Started = true;
		runtime.Emitting = true;
		runtime.Time = 0.0f;
		runtime.EmitAccumulator = 0.0f;
		// The next update adds the loop's BurstCount (it has the component; this may run before it exists).
		runtime.LoopBurstPending = true;
	}

	void ParticleScene::Stop(entt::entity emitter, bool clear)
	{
		auto found = m_Emitters.find(emitter);
		if (found == m_Emitters.end())
		{
			// Not started yet: make sure PlayOnStart doesn't start it.
			EmitterRuntime& runtime = GetOrCreate(emitter);
			runtime.Started = true;
			return;
		}

		EmitterRuntime& runtime = found->second;
		runtime.Started = true;
		runtime.Emitting = false;
		runtime.PendingBurst = 0;
		runtime.LoopBurstPending = false;
		if (clear)
		{
			m_LiveParticles -= std::min(m_LiveParticles, runtime.Particles.Count);
			runtime.Particles.Count = 0;
		}
	}

	void ParticleScene::Burst(entt::entity emitter, uint32_t count)
	{
		EmitterRuntime& runtime = GetOrCreate(emitter);
		runtime.PendingBurst = std::min(runtime.PendingBurst + count, ParticleEmitter2DComponent::MaxParticlesLimit);
	}

	bool ParticleScene::IsPlaying(entt::entity emitter) const
	{
		const EmitterRuntime* runtime = Find(emitter);
		return runtime != nullptr && runtime->Emitting;
	}

	uint32_t ParticleScene::GetAliveCount(entt::entity emitter) const
	{
		const EmitterRuntime* runtime = Find(emitter);
		return runtime != nullptr ? runtime->Particles.Count : 0;
	}

	const ParticleScene::EmitterRuntime* ParticleScene::Find(entt::entity emitter) const
	{
		auto found = m_Emitters.find(emitter);
		return found != m_Emitters.end() ? &found->second : nullptr;
	}

	// ---- System -------------------------------------------------------------------

	void UpdateParticles(Scene& scene, const SystemContext& context)
	{
		if (context.IsPlaying == false)
		{
			return;
		}
		if (ParticleScene* particles = scene.GetParticleScene())
		{
			particles->Update(scene, context);
		}
	}

	// ---- Particles2D --------------------------------------------------------------

	// The scene's ParticleScene when `entity` is a live emitter in a running scene, else nullptr.
	static ParticleScene* FindParticleScene(Entity entity)
	{
		if ((bool)entity == false || entity.GetScene() == nullptr || entity.GetScene()->m_Registry.valid(entity) == false)
		{
			return nullptr;
		}
		if (entity.HasComponent<ParticleEmitter2DComponent>() == false)
		{
			return nullptr;
		}
		return entity.GetScene()->GetParticleScene();
	}

	void Particles2D::Play(Entity entity)
	{
		if (ParticleScene* particles = FindParticleScene(entity))
		{
			particles->Play(entity);
		}
	}

	void Particles2D::Stop(Entity entity, bool clear)
	{
		if (ParticleScene* particles = FindParticleScene(entity))
		{
			particles->Stop(entity, clear);
		}
	}

	void Particles2D::Burst(Entity entity, uint32_t count)
	{
		if (ParticleScene* particles = FindParticleScene(entity))
		{
			particles->Burst(entity, count);
		}
	}

	bool Particles2D::IsPlaying(Entity entity)
	{
		ParticleScene* particles = FindParticleScene(entity);
		return particles != nullptr && particles->IsPlaying(entity);
	}

	uint32_t Particles2D::GetAliveCount(Entity entity)
	{
		ParticleScene* particles = FindParticleScene(entity);
		return particles != nullptr ? particles->GetAliveCount(entity) : 0;
	}
}
