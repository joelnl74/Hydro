#pragma once
#include "EditorContext.h"

#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/RenderView.h>
#include <Hydro/Renderer/SceneRenderer.h>

#include <imgui/imgui.h>

#include <cstdint>

namespace Hydro
{
	// The Game window: the active scene through its primary camera, as the game will show it, and game full screen
	// (F11: the view covers the editor's window, and with FullscreenCoversMonitor the window covers the monitor).
	class GameViewport
	{
	public:
		explicit GameViewport(EditorContext& context);

		void Init();
		// While the GPU is idle (the application waits before detaching layers).
		void Shutdown();

		// Puts the window in or out of full screen (outside the ImGui frame), then renders the view when it is shown.
		void OnUpdate(float deltaTime, SceneRenderer& renderer);
		void OnImGuiRender();
		// Over everything else, so call it last in the frame.
		void DrawFullscreen();
		// F11 toggles full screen, Escape only leaves it.
		void UpdateKeys();

		void SetFullscreen(bool fullscreen);
		bool IsFullscreen() const { return m_Fullscreen; }
		bool IsCapturePending() const { return m_View->IsCapturePending(); }
		uint32_t GetWidth() const { return m_View->GetWidth(); }
		uint32_t GetHeight() const { return m_View->GetHeight(); }

	private:
		// Sizes the view and the scene's viewport to the window. A pending capture is requested here, at the fixed
		// capture size if that option is on.
		void Resize(uint32_t width, uint32_t height);
		// Tells the active scene where the pointer is over the view's image (view pixels, Y up), for the runtime UI.
		void UpdatePointer(const ImVec2& imagePosition, const ImVec2& imageSize);
		// "No primary camera" or "Paused" over the image.
		void DrawStatus(const ImVec2& textPosition);

	private:
		EditorContext& m_Context;
		Scope<RenderView> m_View;
		bool m_Visible = false;
		bool m_SceneHasCamera = true;          // false when the view found no primary camera last frame
		bool m_Fullscreen = false;
		bool m_FocusFullscreen = false;        // bring the full screen view to the front on its first frame
		bool m_WindowFullscreenByGame = false; // the editor's window went full screen for the game, and goes back with it
		int m_PendingWindowFullscreen = -1;    // -1 nothing, 0 / 1: change the window in OnUpdate, outside the ImGui frame
		bool m_FullscreenKeyDown = false;      // F11 last frame, so holding it toggles once
		bool m_EscapeKeyDown = false;
		float m_FullscreenHintTime = 0.0f;     // seconds left to show how to leave full screen
	};
}
