#include "hypch.h"
#include "SpriteAnimator.h"

#include "Hydro/Scene/Components/Components.h"

#include <unordered_set>

namespace Hydro
{
	// ---- Shared by the system and the API ---------------------------------------

	static void StartClip(SpriteAnimatorComponent& animator, int32_t clipIndex)
	{
		animator.ClipIndex = clipIndex;
		animator.FrameIndex = 0;
		animator.FrameTime = 0.0f;
		animator.Step = 1;
		animator.Playing = true;
		animator.Finished = false;
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
				animator.Playing = false;
				animator.Finished = true;
			}
			return;
		}

		switch (clip.Loop)
		{
			case AnimationLoopMode::Once:
			{
				// The last frame has had its time: stay on it.
				if (animator.FrameIndex + 1 < frameCount)
				{
					animator.FrameIndex++;
				}
				else
				{
					animator.Playing = false;
					animator.Finished = true;
				}
				break;
			}
			case AnimationLoopMode::Loop:
			{
				animator.FrameIndex = (animator.FrameIndex + 1) % frameCount;
				break;
			}
			case AnimationLoopMode::PingPong:
			{
				if (animator.Step > 0 && animator.FrameIndex + 1 >= frameCount)
				{
					animator.Step = -1;
				}
				else if (animator.Step < 0 && animator.FrameIndex == 0)
				{
					animator.Step = 1;
				}
				animator.FrameIndex = animator.Step > 0 ? animator.FrameIndex + 1 : animator.FrameIndex - 1;
				break;
			}
		}
	}

	// The entity's animator when it has one with an animation set, else nullptr.
	static SpriteAnimatorComponent* FindAnimator(Entity entity)
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
		return animator.Animations == nullptr ? nullptr : &animator;
	}

	// ---- System -----------------------------------------------------------------

	void UpdateSpriteAnimations(entt::registry& registry, float deltaTime)
	{
		auto view = registry.view<SpriteAnimatorComponent, SpriteRendererComponent>();
		for (auto entity : view)
		{
			auto [animator, sprite] = view.get<SpriteAnimatorComponent, SpriteRendererComponent>(entity);
			if (animator.Animations == nullptr || animator.Animations->GetClipCount() == 0)
			{
				continue;
			}
			const SpriteAnimationSet& set = *animator.Animations;

			if (animator.ClipIndex < 0)
			{
				if (animator.PlayOnStart == false)
				{
					continue;
				}
				StartClip(animator, ResolveStartClip(set, animator.StartClip));
			}

			// A set swapped for one with fewer clips: start over with its default clip.
			if ((uint32_t)animator.ClipIndex >= set.GetClipCount())
			{
				StartClip(animator, (int32_t)set.GetDefaultClip());
			}

			const SpriteAnimationClip& clip = set.GetClip((uint32_t)animator.ClipIndex);
			if (clip.Frames.empty())
			{
				continue;
			}
			if (animator.FrameIndex >= clip.Frames.size())
			{
				animator.FrameIndex = 0;
			}

			if (animator.Playing)
			{
				animator.FrameTime += deltaTime * std::max(animator.Speed, 0.0f);
				// while, not if: a long frame (a hitch, a high Speed) may pass several frames. Durations
				// are at least MinFrameDuration, so this ends.
				while (animator.Playing && animator.FrameTime >= clip.Frames[animator.FrameIndex].Duration)
				{
					animator.FrameTime -= clip.Frames[animator.FrameIndex].Duration;
					AdvanceFrame(animator, clip);
				}
			}

			sprite.Texture = set.GetTexture();
			sprite.SpriteIndex = clip.Frames[animator.FrameIndex].Sprite;
			sprite.FlipX = clip.FlipX;
		}
	}

	// ---- API --------------------------------------------------------------------

	bool SpriteAnimator::Play(Entity entity, std::string_view clip, bool restart)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator == nullptr)
		{
			return false;
		}

		const int32_t index = animator->Animations->FindClip(clip);
		if (index < 0)
		{
			// Play is usually called every frame; say it once per name.
			static std::unordered_set<std::string> s_ReportedClips;
			if (s_ReportedClips.insert(std::string(clip)).second)
			{
				HY_CORE_WARN("SpriteAnimator::Play: no clip '{}' in '{}'", std::string(clip), animator->Animations->GetPath().filename().string());
			}
			return false;
		}

		return Play(entity, (uint32_t)index, restart);
	}

	bool SpriteAnimator::Play(Entity entity, uint32_t clipIndex, bool restart)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator == nullptr || clipIndex >= animator->Animations->GetClipCount())
		{
			return false;
		}

		const bool sameClip = animator->ClipIndex == (int32_t)clipIndex;
		if (sameClip && animator->Playing && restart == false)
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
			animator->Playing = false;
			animator->Finished = false;
			animator->FrameIndex = 0;
			animator->FrameTime = 0.0f;
			animator->Step = 1;
		}
	}

	void SpriteAnimator::Pause(Entity entity)
	{
		if (SpriteAnimatorComponent* animator = FindAnimator(entity))
		{
			animator->Playing = false;
		}
	}

	void SpriteAnimator::Resume(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator != nullptr && animator->ClipIndex >= 0 && animator->Finished == false)
		{
			animator->Playing = true;
		}
	}

	bool SpriteAnimator::IsPlaying(Entity entity, std::string_view clip)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator == nullptr || animator->Playing == false)
		{
			return false;
		}
		return clip.empty() || GetCurrentClip(entity) == clip;
	}

	bool SpriteAnimator::IsFinished(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		return animator != nullptr && animator->Finished;
	}

	std::string_view SpriteAnimator::GetCurrentClip(Entity entity)
	{
		SpriteAnimatorComponent* animator = FindAnimator(entity);
		if (animator == nullptr || animator->ClipIndex < 0 || (uint32_t)animator->ClipIndex >= animator->Animations->GetClipCount())
		{
			return {};
		}
		return animator->Animations->GetClip((uint32_t)animator->ClipIndex).Name;
	}

	void SpriteAnimator::SetSpeed(Entity entity, float speed)
	{
		if (SpriteAnimatorComponent* animator = FindAnimator(entity))
		{
			animator->Speed = std::max(speed, 0.0f);
		}
	}
}
