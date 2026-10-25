#include "hypch.h"
#include "SpriteAnimator.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SpriteAnimation.h"
#include "Hydro/Scene/SystemContext.h"

#include <unordered_set>

namespace Hydro
{
	// ---- Shared by the system and the API ---------------------------------------

	static void StartClip(SpriteAnimatorComponent& animator, int32_t clipIndex)
	{
		animator.State.ClipIndex = clipIndex;
		animator.State.FrameIndex = 0;
		animator.State.FrameTime = 0.0f;
		animator.State.Step = 1;
		animator.State.Playing = true;
		animator.State.Finished = false;
	}

	// The clip to start with: StartClip by name, else the set's default clip.
	static int32_t ResolveStartClip(const SpriteAnimationSet& set, const std::string& startClip)
	{
		if (startClip.empty() == false)
		{
			const int32_t index = set.FindClip(startClip);
			if (index >= 0)
			{
				return index;
			}
			HY_CORE_WARN("SpriteAnimator: start clip '{}' isn't in '{}'; playing the default clip", startClip, set.GetPath().filename().string());
		}
		return (int32_t)set.GetDefaultClip();
	}

	// Moves to the next frame as the clip's loop mode says.
	static void AdvanceFrame(SpriteAnimatorComponent& animator, const SpriteAnimationClip& clip)
	{
		const uint32_t frameCount = (uint32_t)clip.Frames.size();
		if (frameCount <= 1)
		{
			if (clip.Loop == AnimationLoopMode::Once)
			{
				animator.State.Playing = false;
				animator.State.Finished = true;
			}
			return;
		}

		switch (clip.Loop)
		{
			case AnimationLoopMode::Once:
			{
				// The last frame has had its time: stay on it.
				if (animator.State.FrameIndex + 1 < frameCount)
				{
					animator.State.FrameIndex++;
				}
				else
				{
					animator.State.Playing = false;
					animator.State.Finished = true;
				}
				break;
			}
			case AnimationLoopMode::Loop:
			{
				animator.State.FrameIndex = (animator.State.FrameIndex + 1) % frameCount;
				break;
			}
			case AnimationLoopMode::PingPong:
			{
				if (animator.State.Step > 0 && animator.State.FrameIndex + 1 >= frameCount)
				{
					animator.State.Step = -1;
				}
				else if (animator.State.Step < 0 && animator.State.FrameIndex == 0)
				{
					animator.State.Step = 1;
				}
				animator.State.FrameIndex = animator.State.Step > 0 ? animator.State.FrameIndex + 1 : animator.State.FrameIndex - 1;
				break;
			}
		}
	}

	// The entity's animator when it has one whose animation set is loaded, else nullptr; `set` is that
	// set. The AssetManager keeps loaded sets, so it stays valid for the call.
	static SpriteAnimatorComponent* FindAnimator(Entity entity, Ref<SpriteAnimationSet>& set)
	{
		if ((bool)entity == false || entity.GetScene() == nullptr || entity.GetScene()->m_Registry.valid(entity) == false)
		{
			return nullptr;
		}
		if (entity.HasComponent<SpriteAnimatorComponent>() == false)
		{
			return nullptr;
		}

		SpriteAnimatorComponent& animator = entity.GetComponent<SpriteAnimatorComponent>();
		set = AssetManager::GetLoadedAsset<SpriteAnimationSet>(animator.Animations);
		return set == nullptr ? nullptr : &animator;
	}

	static SpriteAnimatorComponent* FindAnimator(Entity entity)
	{
		Ref<SpriteAnimationSet> set;
		return FindAnimator(entity, set);
	}

	// ---- System -----------------------------------------------------------------

	void UpdateSpriteAnimations(Scene& scene, const SystemContext& context)
	{
		if (context.IsPlaying == false)
		{
			return;
		}

		auto view = scene.m_Registry.view<SpriteAnimatorComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			auto [animator, sprite] = view.get<SpriteAnimatorComponent, SpriteRendererComponent>(entity);
			// Not loaded (none assigned, or nobody loaded it): nothing to play. Never loads from disk.
			const Ref<SpriteAnimationSet> loaded = AssetManager::GetLoadedAsset<SpriteAnimationSet>(animator.Animations);
			if (loaded == nullptr || loaded->GetClipCount() == 0)
			{
				continue;
			}
			const SpriteAnimationSet& set = *loaded;

			if (animator.State.ClipIndex < 0)
			{
				if (animator.PlayOnStart == false)
				{
					continue;
				}
				StartClip(animator, ResolveStartClip(set, animator.StartClip));
			}

			// A set swapped for one with fewer clips: start over with its default clip.
			if ((uint32_t)animator.State.ClipIndex >= set.GetClipCount())
			{
				StartClip(animator, (int32_t)set.GetDefaultClip());
			}

			const SpriteAnimationClip& clip = set.GetClip((uint32_t)animator.State.ClipIndex);
			if (clip.Frames.empty())
			{
				continue;
			}
			if (animator.State.FrameIndex >= clip.Frames.size())
			{
				animator.State.FrameIndex = 0;
			}

			if (animator.State.Playing)
			{
				animator.State.FrameTime += context.DeltaTime * std::max(animator.Speed, 0.0f);
				// while, not if: a long frame (a hitch, a high Speed) may pass several frames. Durations
				// are at least MinFrameDuration, so this ends.
				while (animator.State.Playing && animator.State.FrameTime >= clip.Frames[animator.State.FrameIndex].Duration)
				{
					animator.State.FrameTime -= clip.Frames[animator.State.FrameIndex].Duration;
					AdvanceFrame(animator, clip);
				}
			}

			sprite.Texture = set.GetTexture() != nullptr ? set.GetTexture()->GetHandle() : AssetHandle(0);
			sprite.SpriteIndex = clip.Frames[animator.State.FrameIndex].Sprite;
			sprite.FlipX = clip.FlipX;
		}
	}

	void ResetSpriteAnimations(Scene& scene)
	{
		auto view = scene.m_Registry.view<SpriteAnimatorComponent>();
		for (auto entity : view)
		{
			view.get<SpriteAnimatorComponent>(entity).State = {};
		}
	}

	// ---- API --------------------------------------------------------------------

	bool SpriteAnimator::Play(Entity entity, std::string_view clip, bool restart)
	{
		Ref<SpriteAnimationSet> set;
		SpriteAnimatorComponent* animator = FindAnimator(entity, set);
		if (animator == nullptr)
		{
			return false;
		}

		const int32_t index = set->FindClip(clip);
		if (index < 0)
		{
			// Play is usually called every frame; say it once per name.
			static std::unordered_set<std::string> s_ReportedClips;
			if (s_ReportedClips.insert(std::string(clip)).second)
			{
				HY_CORE_WARN("SpriteAnimator::Play: no clip '{}' in '{}'", std::string(clip), set->GetPath().filename().string());
			}
			return false;
		}

		return Play(entity, (uint32_t)index, restart);
	}

	bool SpriteAnimator::Play(Entity entity, uint32_t clipIndex, bool restart)
	{
		Ref<SpriteAnimationSet> set;
		SpriteAnimatorComponent* animator = FindAnimator(entity, set);
		if (animator == nullptr || clipIndex >= set->GetClipCount())
		{
			return false;
		}

		const bool sameClip = animator->State.ClipIndex == (int32_t)clipIndex;
		if (sameClip && animator->State.Playing && restart == false)
		{
			return true;
		}

		StartClip(*animator, (int32_t)clipIndex);
		return true;
	}

	void SpriteAnimator::Stop(Entity entity)
	{
		if (SpriteAnimatorComponent* animator = FindAnimator(entity))
		{
			animator->State.Playing = false;
			animator->State.Finished = false;
			animator->State.FrameIndex = 0;
			animator->State.FrameTime = 0.0f;
			animator->State.Step = 1;
		}
	}

	void SpriteAnimator::Pause(Entity entity)
	{
		if (SpriteAnimatorComponent* animator = FindAnimator(entity))
		{
			animator->State.Playing = false;
		}
	}

	void SpriteAnimator::Resume(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator != nullptr && animator->State.ClipIndex >= 0 && animator->State.Finished == false)
		{
			animator->State.Playing = true;
		}
	}

	bool SpriteAnimator::IsPlaying(Entity entity, std::string_view clip)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator == nullptr || animator->State.Playing == false)
		{
			return false;
		}
		return clip.empty() || GetCurrentClip(entity) == clip;
	}

	bool SpriteAnimator::IsFinished(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		return animator != nullptr && animator->State.Finished;
	}

	std::string_view SpriteAnimator::GetCurrentClip(Entity entity)
	{
		Ref<SpriteAnimationSet> set;
		SpriteAnimatorComponent* animator = FindAnimator(entity, set);
		if (animator == nullptr || animator->State.ClipIndex < 0 || (uint32_t)animator->State.ClipIndex >= set->GetClipCount())
		{
			return {};
		}
		return set->GetClip((uint32_t)animator->State.ClipIndex).Name; // the manager keeps the set, so the name outlives this call
	}

	void SpriteAnimator::SetSpeed(Entity entity, float speed)
	{
		if (SpriteAnimatorComponent* animator = FindAnimator(entity))
		{
			animator->Speed = std::max(speed, 0.0f);
		}
	}

	uint32_t SpriteAnimator::GetFrameIndex(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		return animator != nullptr && animator->State.ClipIndex >= 0 ? animator->State.FrameIndex : 0;
	}

	void SpriteAnimator::ResetPlayback(Entity entity)
	{
		// No FindAnimator: the new set may not be loaded (or the handle was cleared).
		if ((bool)entity == false || entity.GetScene() == nullptr || entity.GetScene()->m_Registry.valid(entity) == false)
		{
			return;
		}
		if (entity.HasComponent<SpriteAnimatorComponent>())
		{
			entity.GetComponent<SpriteAnimatorComponent>().State = {};
		}
	}

	void SpriteAnimator::RestartAfterReload(Entity entity, const SpriteAnimationSet* before)
	{
		Ref<SpriteAnimationSet> set;
		SpriteAnimatorComponent* animator = FindAnimator(entity, set);
		if (animator == nullptr)
		{
			return;
		}

		SpriteAnimatorState& state = animator->State;
		const bool wasValid = before != nullptr && state.ClipIndex >= 0 && (uint32_t)state.ClipIndex < before->GetClipCount();
		const std::string playing = wasValid ? before->GetClip((uint32_t)state.ClipIndex).Name : std::string();
		state.ClipIndex = playing.empty() ? -1 : set->FindClip(playing);
		state.FrameIndex = 0;
		state.FrameTime = 0.0f;
	}
}
