#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/SpriteAnimation.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace Hydro
{
	// Edits a .hyanim (a SpriteAnimationSet): its sheet, the clips (name, loop mode, flip, frames with
	// durations, events) and the default clip, with a live preview and a sprite picker (click a
	// sprite to add it as a frame). Save writes the file and asks the editor to reload the animators
	// that use it. Opened by double-clicking a .hyanim or from its inspector.
	class SpriteAnimationWindow
	{
	public:
		// Set by the editor layer: the .hyanim was saved.
		std::function<void(const std::filesystem::path&)> OnSaved;

		// Shows `path` (asks first when the open one has unsaved changes).
		void Open(const std::filesystem::path& path);
		void OnImguiRender();
		bool& IsOpen() { return m_Open; }

		// One clip per group of sprites named "<group>_<number>" (e.g. Farmer_Left_0..5 -> "Farmer_Left"),
		// frames in sprite order, each lasting 1 / frameRate seconds. Also used by the Project window's
		// "Create Sprite Animation".
		static std::vector<SpriteAnimationClip> MakeClipsFromSpriteNames(const Texture2D& texture, float frameRate);
		// The frame a preview shows `time` seconds into the clip, following its loop mode.
		static uint32_t GetFrameAtTime(const SpriteAnimationClip& clip, float time);

	private:
		void Load(const std::filesystem::path& path);
		void Save();
		void SetTexture(const Ref<Texture2D>& texture);

		void DrawToolbar();
		void DrawClipList();
		void DrawClip();
		void DrawFrames(SpriteAnimationClip& clip);
		void DrawEvents(SpriteAnimationClip& clip);
		void DrawPreview();
		void DrawSpritePicker();
		void DrawDiscardPopup();

		SpriteAnimationClip* GetSelectedClip();

	private:
		bool m_Open = false;
		std::filesystem::path m_Path;
		bool m_Loaded = false;
		Ref<Texture2D> m_Texture;
		std::vector<SpriteAnimationClip> m_Clips; // being edited
		int32_t m_DefaultClip = 0;
		int32_t m_SelectedClip = -1;
		int32_t m_SelectedFrame = -1;
		bool m_Dirty = false;

		float m_PreviewTime = 0.0f;
		float m_PreviewSpeed = 1.0f;
		bool m_PreviewPlaying = true;
		float m_FrameRateForAll = 10.0f;
		char m_SpriteFilter[64] = {};

		std::filesystem::path m_PendingOpen; // waiting for "discard changes?"
		bool m_RequestDiscardPopup = false;
	};
}
