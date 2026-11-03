#pragma once
#include <cstdint>

namespace Hydro
{
	struct RenderViewData;

	// Uploads what a view is lit with for one frame: the sun, ambient, fog and shadow settings (FrameLightingUniform,
	// set 0 binding 2) and the point and spot lights reaching nearest to what the camera looks at (binding 3).
	class SceneLightingBuffer
	{
	public:
		// A view that isn't forward lit this frame uploads zeros, and the shaders draw it unlit.
		static void Upload(RenderViewData& view, uint32_t frame);
	};
}
