#pragma once

namespace Hydro
{
	// A scene's post-processing (post-processing plan P1, P2; render profiles in P4; depth of field #135), saved in the scene
	// (SceneSerializer, "PostProcess") only when it differs from the defaults, and copied on Play. Read by views
	// whose render features have HDR (and Bloom, DepthOfField) on; the 8-bit path (the 2D preset) ignores it.
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

		// Depth of field (with the DepthOfField render feature): what lies far in front of or behind the camera's
		// focus point blurs, so the band the player walks in stays sharp and the foreground and background soften
		// (the tilt-shift look of a photographed diorama). Distances are metres along the view direction from where
		// the camera's view meets the ground (z = 0), measured as the fog is (lighting.glsl ApplyFog), but on both
		// sides: in front of that point (towards the camera) and behind it.
		bool DepthOfField = true;  // a quick off switch for comparisons; the feature stays on
		float DofNearStart = 8.0f; // metres in front of the focus point that stay sharp
		float DofFarStart = 6.0f;  // metres behind the focus point that stay sharp
		float DofRange = 10.0f;    // metres beyond either start over which the blur grows to its largest
		float DofMaxRadius = 0.6f; // the largest blur radius, in percent of the view's height (0.6 = 6.5 px at 1080 lines)

		bool operator==(const ScenePostProcess& other) const
		{
			return Exposure == other.Exposure && Bloom == other.Bloom && BloomThreshold == other.BloomThreshold
				&& BloomKnee == other.BloomKnee && BloomIntensity == other.BloomIntensity && BloomRadius == other.BloomRadius
				&& DepthOfField == other.DepthOfField && DofNearStart == other.DofNearStart && DofFarStart == other.DofFarStart
				&& DofRange == other.DofRange && DofMaxRadius == other.DofMaxRadius;
		}
		bool operator!=(const ScenePostProcess& other) const { return (*this == other) == false; }
	};
}
