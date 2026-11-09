#pragma once
#include "Hydro/Scene/Entity.h"

#include <entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	class Scene;
	struct SystemContext;
	struct ParticleEmitter2DComponent;

	// The live particles and playback state of one running scene, per emitter entity (the component holds settings
	// only). Exists while the game runs; UpdateParticles advances it, SceneRenderer reads it, Particles2D controls it.
	class ParticleSimulation2D
	{
	public:
		// Across all emitters; new particles past it are dropped and counted.
		static constexpr uint32_t MaxLiveParticles = 8192;

		// One emitter's particles, structure of arrays. Live ones are packed at [0, Count); a dead one is swapped with
		// the last, so order changes.
		struct ParticleBuffer
		{
			std::vector<glm::vec2> Position;  // world units
			std::vector<glm::vec2> Velocity;  // world units per second
			std::vector<float> Age;           // seconds since birth
			std::vector<float> Lifetime;      // seconds
			std::vector<float> StartSize;     // world units
			std::vector<float> Rotation;      // radians
			std::vector<float> AngularVelocity; // radians per second
			uint32_t Count = 0;

			uint32_t GetCapacity() const { return (uint32_t)Age.size(); }
		};

		// Playback of one emitter.
		struct EmitterRuntime
		{
			ParticleBuffer Particles;
			bool Started = false;       // PlayOnStart was applied, or game code took control
			bool Emitting = false;
			bool HasEmitted = false;    // emitted at least once (DestroyWhenFinished waits for it)
			float Time = 0.0f;          // seconds into the current loop
			float EmitAccumulator = 0.0f; // fractional particles carried to the next frame
			uint32_t PendingBurst = 0;  // particles to emit at the next update (loop start, Burst)
			bool LoopBurstPending = false; // Play was called: add the emitter's BurstCount at the next update
			uint32_t Random = 0;        // xorshift state
		};

		struct Statistics
		{
			uint32_t Emitters = 0;          // emitters with a runtime entry
			uint32_t LiveParticles = 0;
			uint32_t ReservedParticles = 0; // capacity of all buffers, in use or kept for reuse
			uint32_t DroppedParticles = 0;  // this update: over MaxParticles or MaxLiveParticles
			float UpdateMs = 0.0f;          // CPU time of the last UpdateParticles
		};

		ParticleSimulation2D() = default;
		ParticleSimulation2D(const ParticleSimulation2D&) = delete;
		ParticleSimulation2D& operator=(const ParticleSimulation2D&) = delete;

		// Emits, simulates and retires every emitter's particles, then destroys finished DestroyWhenFinished entities
		// and drops entries of emitters that are gone.
		void Update(Scene& scene, const SystemContext& context);

		// Drops every emitter's particles and playback; buffers are kept for reuse.
		void Clear();

		// Game control (Particles2D); works before the emitter's first update too.
		void Play(entt::entity emitter);
		void Stop(entt::entity emitter, bool clear);
		void Burst(entt::entity emitter, uint32_t count);
		bool IsPlaying(entt::entity emitter) const;
		uint32_t GetAliveCount(entt::entity emitter) const;

		// The emitter's playback and particles, or nullptr when it has none yet (SceneRenderer).
		const EmitterRuntime* Find(entt::entity emitter) const;
		const Statistics& GetStats() const { return m_Stats; }

	private:
		EmitterRuntime& GetOrCreate(entt::entity emitter);
		// Grows the buffer to `capacity`, keeping live particles. Allocates only on growth, never in the steady state.
		void Reserve(EmitterRuntime& runtime, uint32_t capacity);
		void ReleaseBuffer(ParticleBuffer& buffer);
		// Emits `count` particles at `origin` (the entity's world position). Returns how many were dropped.
		uint32_t Emit(EmitterRuntime& runtime, const ParticleEmitter2DComponent& emitter, const glm::vec2& origin, uint32_t count);

		std::unordered_map<entt::entity, EmitterRuntime> m_Emitters;
		// Buffers of emitters that are gone, reused by new ones so bursts of effects don't allocate.
		std::vector<ParticleBuffer> m_FreeBuffers;
		// Entities to destroy after the update's view loop; kept to reuse its capacity.
		std::vector<entt::entity> m_Finished;
		uint32_t m_LiveParticles = 0;
		uint32_t m_PlaybackCounter = 0; // makes seed 0 differ on every playback
		Statistics m_Stats;
	};

	// Stage 5 (after transforms, play mode only): updates the scene's ParticleSimulation2D.
	void UpdateParticles(Scene& scene, const SystemContext& context);

	// Controls an entity's ParticleEmitter2DComponent from game code (Particles2D::Burst(GetEntity(), 20)).
	// Ignored without an emitter or a running scene; shows at the next UpdateParticles.
	class Particles2D
	{
	public:
		// Starts emitting from the start of a loop (its BurstCount included), also after Stop.
		static void Play(Entity entity);
		// Stops emitting; live particles finish their life unless `clear`.
		static void Stop(Entity entity, bool clear = false);
		// Emits `count` particles at once, whether or not the emitter is playing.
		static void Burst(Entity entity, uint32_t count);
		static bool IsPlaying(Entity entity);
		static uint32_t GetAliveCount(Entity entity);
	};
}
