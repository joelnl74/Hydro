#pragma once
#include "Hydro/Scene/Entity.h"

#include <entt.hpp>

#include <cstdint>
#include <string_view>

namespace Hydro
{
	// System (play mode, right after native scripts): advances every SpriteAnimatorComponent by
	// deltaTime * Speed and writes the current frame into the entity's SpriteRendererComponent
	// (Texture, SpriteIndex, FlipX). Starts PlayOnStart animators on their first update.
	void UpdateSpriteAnimations(entt::registry& registry, float deltaTime);

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
		static void SetSpeed(Entity entity, float speed);
	};
}
