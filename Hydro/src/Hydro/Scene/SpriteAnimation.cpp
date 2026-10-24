#include "hypch.h"
#include "SpriteAnimation.h"
#include "SpriteAnimationSerializer.h"

#include "Hydro/Asset/AssetManager.h"

#include <algorithm>

namespace Hydro
{
	// Sets loaded from files that are still alive, so animators using one file share one set.
	// Weak: the cache never keeps a set alive by itself.
	// TODO(assets): replaced by the AssetManager's handle -> asset table.
	static std::unordered_map<std::string, std::weak_ptr<SpriteAnimationSet>> s_LoadedAnimationSets;

	float SpriteAnimationClip::GetLength() const
	{
		float length = 0.0f;
		for (const SpriteAnimationFrame& frame : Frames)
		{
			length += frame.Duration;
		}
		return length;
	}

	SpriteAnimationSet::SpriteAnimationSet(const SpriteAnimationSetSpecification& specification)
		: m_Texture(specification.Texture), m_Clips(specification.Clips)
	{
		for (uint32_t i = 0; i < (uint32_t)m_Clips.size(); i++)
		{
			SpriteAnimationClip& clip = m_Clips[i];

			// Durations are clamped so UpdateSpriteAnimations always moves forward.
			for (SpriteAnimationFrame& frame : clip.Frames)
			{
				frame.Duration = std::max(frame.Duration, SpriteAnimationClip::MinFrameDuration);
			}

			if (m_ClipIndices.emplace(clip.Name, i).second == false)
			{
				HY_CORE_WARN("SpriteAnimationSet: two clips are named '{}'; Play finds the first", clip.Name);
			}
		}

		m_DefaultClip = m_Clips.empty() ? 0 : std::min(specification.DefaultClip, (uint32_t)m_Clips.size() - 1);
	}

	int32_t SpriteAnimationSet::FindClip(std::string_view name) const
	{
		const auto found = m_ClipIndices.find(std::string(name));
		return found == m_ClipIndices.end() ? -1 : (int32_t)found->second;
	}

	Ref<SpriteAnimationSet> SpriteAnimationSet::LoadFromFile(const std::filesystem::path& path, bool reload)
	{
		std::error_code error;
		std::filesystem::path absolutePath = std::filesystem::absolute(path, error).lexically_normal();
		if (error)
		{
			absolutePath = path;
		}

		// The modification time is part of the key, so a re-saved file loads again.
		const auto writeTime = std::filesystem::last_write_time(absolutePath, error);
		const std::string key = absolutePath.generic_u8string() + "|" +
			std::to_string(error ? 0 : (int64_t)writeTime.time_since_epoch().count());
		if (auto found = s_LoadedAnimationSets.find(key); reload == false && found != s_LoadedAnimationSets.end())
		{
			if (Ref<SpriteAnimationSet> set = found->second.lock())
			{
				return set;
			}
		}

		SpriteAnimationSetSpecification specification;
		if (SpriteAnimationSerializer::Deserialize(absolutePath, specification) == false)
		{
			return nullptr;
		}

		Ref<SpriteAnimationSet> set = CreateRef<SpriteAnimationSet>(specification);
		set->m_Path = absolutePath;
		AssetManager::BindHandle(*set, absolutePath);

		for (auto it = s_LoadedAnimationSets.begin(); it != s_LoadedAnimationSets.end();)
		{
			if (it->second.expired())
			{
				it = s_LoadedAnimationSets.erase(it);
			}
			else
			{
				++it;
			}
		}
		s_LoadedAnimationSets[key] = set;

		return set;
	}
}
