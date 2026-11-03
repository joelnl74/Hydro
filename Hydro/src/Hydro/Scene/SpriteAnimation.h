#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/Texture2D.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Hydro
{
	enum class AnimationLoopMode : uint8_t
	{
		Once = 0, // stops on the last frame
		Loop,     // last frame -> first frame
		PingPong  // forwards, then backwards, and again
	};

	// One keyframe: a sprite of the set's texture, shown for Duration seconds.
	struct SpriteAnimationFrame
	{
		uint32_t Sprite = 0;   // index into the texture's sprites (Texture2D::GetSprite)
		float Duration = 0.1f; // seconds, at least MinFrameDuration
	};

	// A name reached when its frame starts (footsteps, the hit frame of an attack).
	struct SpriteAnimationEvent
	{
		uint32_t Frame = 0;
		std::string Name;
	};

	struct SpriteAnimationClip
	{
		static constexpr float MinFrameDuration = 0.001f;

		std::string Name;
		std::vector<SpriteAnimationFrame> Frames;
		std::vector<SpriteAnimationEvent> Events;
		AnimationLoopMode Loop = AnimationLoopMode::Loop;
		bool FlipX = false; // e.g. "WalkLeft" made from the right-facing frames

		// Seconds for one pass through the frames.
		float GetLength() const;
	};

	struct SpriteAnimationSetSpecification
	{
		// The sprite sheet every clip's frames come from (a Multiple texture). TODO(assets): becomes an AssetHandle.
		Ref<Texture2D> Texture;
		std::vector<SpriteAnimationClip> Clips;
		uint32_t DefaultClip = 0;
	};

	// Named clips over one texture's sprites, from a .hyanim file. Give every character the same clip names ("WalkLeft")
	// so one script drives any of them. Immutable once made; shared in a Ref.
	class SpriteAnimationSet : public Asset
	{
	public:
		explicit SpriteAnimationSet(const SpriteAnimationSetSpecification& specification);

		SpriteAnimationSet(const SpriteAnimationSet&) = delete;
		SpriteAnimationSet& operator=(const SpriteAnimationSet&) = delete;

		// Reads a .hyanim file and its texture into a new, unshared set; null (logged) on failure. The asset loader uses it.
		static Ref<SpriteAnimationSet> LoadUncached(const std::filesystem::path& path);

		// Reads a .hyanim file and its texture; null (logged) on failure. Asset-folder files are shared by the AssetManager,
		// others while a set from them is alive. `reload` reads them again even when shared (after a re-slice).
		static Ref<SpriteAnimationSet> LoadFromFile(const std::filesystem::path& path, bool reload = false);

		static AssetType GetStaticType() { return AssetType::SpriteAnimation; }
		AssetType GetType() const override { return GetStaticType(); }

		const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		uint32_t GetClipCount() const { return (uint32_t)m_Clips.size(); }
		const SpriteAnimationClip& GetClip(uint32_t index) const { return m_Clips[index]; }
		// -1 when there is no clip with that name.
		int32_t FindClip(std::string_view name) const;
		// Index of the clip played when an animator names none; only valid when GetClipCount() > 0.
		uint32_t GetDefaultClip() const { return m_DefaultClip; }
		// `startClip` by name, else the default clip; -1 without clips. Quiet, so editors can show the start frame.
		int32_t FindStartClip(std::string_view startClip) const;
		// The .hyanim file it was loaded from (absolute), or empty for sets made in code.
		const std::filesystem::path& GetPath() const { return m_Path; }

	private:
		Ref<Texture2D> m_Texture;
		std::vector<SpriteAnimationClip> m_Clips;
		std::unordered_map<std::string, uint32_t> m_ClipIndices;
		uint32_t m_DefaultClip = 0;
		std::filesystem::path m_Path;
	};
}
