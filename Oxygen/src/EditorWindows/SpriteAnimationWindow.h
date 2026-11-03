#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/SpriteAnimation.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace Hydro
{
	// Edits a .hyanim: its sheet, clips (loop mode, flip, frames, events) and default clip, with a live preview.
	// Save writes the file and asks the editor to reload the animators that use it.
	class SpriteAnimationWindow
	{
	public:
		// Called after the .hyanim was saved.
		std::function<void(const std::filesystem::path&)> OnSaved;

		// Shows `path`, asking first when the open one has unsaved changes.
		void Open(const std::filesystem::path& path);
		void OnImguiRender();
		bool& IsOpen() { return m_Open; }

		// The .hyanim shown follows a rename or move in the Project window.
		void OnAssetMoved(const std::filesystem::path& from, const std::filesystem::path& to);
		// Closes the window when its file was deleted, so Save doesn't write it back.
		void OnAssetDeleted(const std::filesystem::path& path);

		// One clip per group of sprites named "<group>_<number>" (Farmer_Left_0..5 -> "Farmer_Left"), each frame
		// lasting 1 / frameRate seconds.
		static std::vector<SpriteAnimationClip> MakeClipsFromSpriteNames(const Texture2D& texture, float frameRate);
		// The frame shown `time` seconds into the clip, following its loop mode.
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
