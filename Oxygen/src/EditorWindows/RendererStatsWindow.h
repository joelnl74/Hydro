#pragma once

namespace Hydro
{
	// Renderer2D statistics. Drawn as the "Rendering" tab of the Stats window
	// (StatsWindow), so it has no ImGui window of its own.
	class RendererStatsPanel
	{
	public:
		void Draw();
	};
}
