#pragma once

namespace Hydro
{
	// A scene's post-processing (post-processing plan P1; bloom joins in P2, render profiles in P4), saved in the
	// scene (SceneSerializer, "PostProcess") only when it differs from the defaults, and copied on Play. Read by
	// views whose render features have HDR on; the 8-bit path (the 2D preset) ignores it.
	struct ScenePostProcess
	{
		// Manual exposure in EV (stops): the HDR scene colour is multiplied by 2^Exposure before the tonemap.
		// 0 = as lit; +1 twice as bright, -1 half.
		float Exposure = 0.0f;

		bool operator==(const ScenePostProcess& other) const { return Exposure == other.Exposure; }
		bool operator!=(const ScenePostProcess& other) const { return (*this == other) == false; }
	};
}
