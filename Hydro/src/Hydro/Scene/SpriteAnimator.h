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

	// System (play mode, after physics and before transforms): advances every SpriteAnimatorComponent by
	// context.DeltaTime * Speed and writes the current frame into the entity's SpriteRendererComponent
	// (Texture, SpriteIndex, FlipX). Starts PlayOnStart animators on their first update.
	// Does nothing unless context.IsPlaying.
	void UpdateSpriteAnimations(Scene& scene, const SystemContext& context);

	// Clears every animator's playback (SpriteAnimatorState), so the game starts from PlayOnStart and
	// StartClip whatever the editor scene's copy held. Scene::OnRuntimeStart calls it.
	void ResetSpriteAnimations(Scene& scene);

	// Controls an entity's SpriteAnimatorComponent from scripts and game code:
	//
	//     SpriteAnimator::Play(GetEntity(), moving ? "WalkLeft" : "IdleLeft");
	//
	// Plain functions over the component (no state of its own); entities without a
	// SpriteAnimatorComponent or without an animation set are ignored. The new frame shows when
	// UpdateSpriteAnimations runs, which is later in the same frame when called from a script.
	class SpriteAnimator
	{
	public:
		// Starts the clip from its first frame. Already playing that clip: it carries on, so this is
		// safe to call every frame, unless `restart`. A paused or finished clip starts again. An
		// unknown name logs a warning (once per name) and returns false.
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
		// Sets the saved Speed (clamped to 0 or more). Meant for play mode, where the scene is a copy:
		// called on the editor scene it changes what is saved.
		static void SetSpeed(Entity entity, float speed);

		// Clears the playback, as if the animator had never started (the animation set changed in the
		// inspector). Works without a loaded set.
		static void ResetPlayback(Entity entity);
		// After the entity's animation set was reloaded (edited and saved in the Animation window):
		// keeps the clip with the same name as the one that was playing in `before`, from its first
		// frame, or starts over (PlayOnStart) when the new set has no such clip.
		static void RestartAfterReload(Entity entity, const SpriteAnimationSet* before);
	};
}
