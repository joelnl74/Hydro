#pragma once

namespace Hydro
{
	// A scene's post-processing (post-processing plan P1, P2; render profiles in P4), saved in the scene
	// (SceneSerializer, "PostProcess") only when it differs from the defaults, and copied on Play. Read by views
	// whose render features have HDR (and Bloom) on; the 8-bit path (the 2D preset) ignores it.
	struct ScenePostProcess
	{
		// Manual exposure in EV (stops): the HDR scene colour is multiplied by 2^Exposure before the tonemap.
		// 0 = as lit; +1 twice as bright, -1 half.
		float Exposure = 0.0f;

		// Bloom (with the Bloom render feature): a glow around pixels brighter than BloomThreshold.
		bool Bloom = true;            // a quick off switch for comparisons; the feature stays on
		float BloomThreshold = 1.0f;  // linear HDR brightness (its brightest channel) where the glow starts
		float BloomKnee = 0.5f;       // 0..1 of the threshold: how softly it starts (no hard edge, no flicker)
		float BloomIntensity = 0.6f;  // how much of the glow is added
		float BloomRadius = 1.0f;     // how far each level spreads (the upsample filter's size), 0.5..2

		bool operator==(const ScenePostProcess& other) const
		{
			return Exposure == other.Exposure && Bloom == other.Bloom && BloomThreshold == other.BloomThreshold
				&& BloomKnee == other.BloomKnee && BloomIntensity == other.BloomIntensity && BloomRadius == other.BloomRadius;
		}
		bool operator!=(const ScenePostProcess& other) const { return (*this == other) == false; }
	};
}
