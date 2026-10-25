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

	// The live particles of one scene: per emitter entity, a buffer of particles and the playback state
	// (component state rule 3 in docs/SceneAndECS.md: ParticleEmitter2DComponent holds settings only).
	// A per-scene counterpart like PhysicsScene: Scene::OnRuntimeStart makes it, OnRuntimeStop and the
	// scene's destructor drop it, so Play starts empty and nothing is saved or copied.
	// UpdateParticles advances it; SceneRenderer reads it; Particles2D controls it from game code.
	class ParticleScene
	{
	public:
		// Live particles of all emitters together, at most (new particles past it are dropped and counted).
		static constexpr uint32_t MaxLiveParticles = 8192;

		// One emitter's particles, structure of arrays. Live particles are packed at [0, Count); a dead
		// one is replaced by the last (swap-remove), so their order changes. Capacity is the arrays' size.
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

		ParticleScene() = default;
		ParticleScene(const ParticleScene&) = delete;
		ParticleScene& operator=(const ParticleScene&) = delete;

		// Emits, simulates and retires particles for every ParticleEmitter2DComponent, then destroys the
		// finished DestroyWhenFinished entities (after the view loop) and drops entries of emitters
		// that no longer exist. Called by UpdateParticles.
		void Update(Scene& scene, const SystemContext& context);

		// Drops every emitter's particles and playback; buffers are kept for reuse.
		void Clear();

		// Game control (Particles2D). Work before the emitter's first update too.
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
		// Makes sure the buffer holds `capacity` particles, keeping the live ones. May allocate (an
		// emitter's first update or a larger MaxParticles), never in the steady state.
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

	// System, stage 5 (after transforms, play mode only): ParticleScene::Update on the scene's
	// ParticleScene. Does nothing while editing (editor previews are Roadmap V6) or without one.
	void UpdateParticles(Scene& scene, const SystemContext& context);

	// Controls an entity's ParticleEmitter2DComponent from scripts and game code, like Physics2D and
	// SpriteAnimator: plain functions that route to the scene's ParticleScene. Entities without an emitter,
	// or a scene that isn't running, are ignored. The effect shows at the next UpdateParticles, later the
	// same frame when called from a script.
	//
	//     Particles2D::Burst(GetEntity(), 20);
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
