#pragma once
#include <array>

namespace Hydro
{
	// Small editor panel showing Renderer2D statistics for the last rendered frame.
	class RendererStatsWindow
	{
	public:
		void OnImguiRender();

	private:
		static constexpr int HistorySize = 120; // ~2 seconds at 60 fps

		std::array<float, HistorySize> m_FrameTimes{};
		int m_FrameTimeOffset = 0;
		bool m_Open = true;
	};
}
