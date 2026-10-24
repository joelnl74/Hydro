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
		// The sprite sheet every clip's frames come from (a Multiple texture).
		// TODO(assets): becomes an AssetHandle.
		Ref<Texture2D> Texture;
		std::vector<SpriteAnimationClip> Clips;
		uint32_t DefaultClip = 0;
	};

	// Named clips over the sprites of one texture, loaded from a .hyanim file
	// (SpriteAnimationSerializer). The clip name is what SpriteAnimator::Play takes, so give every
	// character its own set with the same clip names ("IdleDown", "WalkLeft", ...) and one script can
	// drive any of them.
	//
	// A resource: made from a specification and immutable afterwards (the editor saves a new file and
	// loads it again); shared by every animator that uses it, so held in a Ref.
	class SpriteAnimationSet : public Asset
	{
	public:
		explicit SpriteAnimationSet(const SpriteAnimationSetSpecification& specification);

		SpriteAnimationSet(const SpriteAnimationSet&) = delete;
		SpriteAnimationSet& operator=(const SpriteAnimationSet&) = delete;

		// Reads a .hyanim file and its texture, without sharing: every call makes a new set. Returns
		// nullptr (logged) when either can't be read. The AssetManager's loader calls this.
		static Ref<SpriteAnimationSet> LoadUncached(const std::filesystem::path& path);

		// Reads a .hyanim file and its texture. Returns nullptr (logged) when either can't be read.
		// A file in the open project's asset folder is an asset: the AssetManager loads it once and
		// shares it until the project closes (AssetManager::GetAsset<SpriteAnimationSet>(handle)); the
		// set then knows its handle. A file anywhere else is shared while a set from it (same
		// modification time) is alive.
		// `reload` reads the file (and its texture's current import) again even when it is shared,
		// e.g. after the texture was re-sliced; later loads then share the new set.
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
		// The clip an animator starts with: `startClip` by name, else the default clip. -1 when the set has
		// no clips. Quiet: editors use it to show the start frame.
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
