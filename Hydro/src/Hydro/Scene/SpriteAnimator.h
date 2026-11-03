#pragma once
#include "Hydro/Scene/Entity.h"

#include <entt.hpp>

#include <cstdint>
#include <string_view>

namespace Hydro
{
	class Scene;
	class SpriteAnimationSet;
	struct SystemContext;

	// Play mode, after physics and before transforms: advances every SpriteAnimatorComponent and writes the current
	// frame into its SpriteRendererComponent (Texture, SpriteIndex, FlipX). Starts PlayOnStart animators.
	void UpdateSpriteAnimations(Scene& scene, const SystemContext& context);

	// Clears every animator's playback so the game starts from PlayOnStart / StartClip. Scene::OnRuntimeStart calls it.
	void ResetSpriteAnimations(Scene& scene);

	// Controls an entity's SpriteAnimatorComponent from game code (SpriteAnimator::Play(GetEntity(), "WalkLeft")).
	// No state of its own; entities without an animator or set are ignored. Shows at the next UpdateSpriteAnimations.
	class SpriteAnimator
	{
	public:
		// Starts the clip from its first frame; a clip already playing carries on unless `restart`, so it is safe every
		// frame. An unknown name warns once and returns false.
		static bool Play(Entity entity, std::string_view clip, bool restart = false);
		static bool Play(Entity entity, uint32_t clipIndex, bool restart = false);
		// Stops and goes back to the clip's first frame (which stays on screen).
		static void Stop(Entity entity);
		// Freezes on the current frame; Resume carries on from there.
		static void Pause(Entity entity);
		static void Resume(Entity entity);

		// Playing at all, or (with a name) playing that clip.
		static bool IsPlaying(Entity entity, std::string_view clip = {});
		// A Once clip reached its last frame.
		static bool IsFinished(Entity entity);
		// The current clip's name; empty when nothing was started.
		static std::string_view GetCurrentClip(Entity entity);
		// The frame of the current clip that is showing (0 when nothing was started).
		static uint32_t GetFrameIndex(Entity entity);
		// Sets the saved Speed (at least 0). Meant for play mode: on the editor scene it changes what is saved.
		static void SetSpeed(Entity entity, float speed);

		// Clears the playback as if it never started (the set changed in the inspector). Works without a loaded set.
		static void ResetPlayback(Entity entity);
		// After the set was reloaded: keeps the clip named as the one playing in `before` (from its first frame), or
		// starts over when the new set has no such clip.
		static void RestartAfterReload(Entity entity, const SpriteAnimationSet* before);
	};
}
