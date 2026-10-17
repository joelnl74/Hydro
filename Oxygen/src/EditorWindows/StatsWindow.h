#pragma once
#include "RendererStatsWindow.h"

#include <Hydro/Core/Base.h>
#include <array>

namespace Hydro
{
	class Scene;

	// Main "Stats" window: frame rate / frame time benchmarks at the top,
	// with tabs for the frame's CPU/GPU breakdown, rendering and the scene.
	class StatsWindow
	{
	public:
		void OnImguiRender(const Ref<Scene>& scene);

		bool& IsOpen() { return m_Open; }

	private:
		void RecordFrame(float frameMs);
		void DrawSummary();
		void DrawFrameTab();
		void DrawSceneTab(const Ref<Scene>& scene);

	private:
		static constexpr int HistorySize = 240; // ~4 seconds at 60 fps

		std::array<float, HistorySize> m_FrameTimes{}; // ms, ring buffer
		int m_HistoryOffset = 0;
		int m_HistoryCount = 0;

		// Summary over the history window (recomputed every frame).
		float m_AvgMs = 0.0f, m_MinMs = 0.0f, m_MaxMs = 0.0f, m_OnePercentLowFps = 0.0f;

		bool m_Open = true;
		bool m_Paused = false;

		RendererStatsPanel m_RendererPanel;
	};
}
