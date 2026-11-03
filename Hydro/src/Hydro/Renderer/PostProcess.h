#pragma once

#include <glm/glm.hpp>

namespace Hydro
{
	// A scene's post-processing, saved only when it differs from the defaults and copied on Play. Read by views with
	// HDR on (and Bloom, DepthOfField); the 8-bit path of the 2D preset ignores it.
	struct ScenePostProcess
	{
		// Manual exposure in EV: the HDR colour is multiplied by 2^Exposure before the tonemap (0 = as lit).
		float Exposure = 0.0f;

		// Bloom (with the Bloom render feature): a glow around pixels brighter than BloomThreshold.
		bool Bloom = true;            // a quick off switch for comparisons; the feature stays on
		float BloomThreshold = 1.0f;  // linear HDR brightness (its brightest channel) where the glow starts
		float BloomKnee = 0.5f;       // 0..1 of the threshold: how softly it starts (no hard edge, no flicker)
		float BloomIntensity = 0.6f;  // how much of the glow is added
		float BloomRadius = 1.0f;     // how far each level spreads (the upsample filter's size), 0.5..2

		// Depth of field: what lies far in front of or behind the focus point blurs (the tilt-shift diorama look).
		// Distances are metres along the view from where the view meets the ground, on both sides of it.
		bool DepthOfField = true;  // a quick off switch for comparisons; the feature stays on
		float DofNearStart = 8.0f; // metres in front of the focus point that stay sharp
		float DofFarStart = 6.0f;  // metres behind the focus point that stay sharp
		float DofRange = 10.0f;    // metres beyond either start over which the blur grows to its largest
		float DofMaxRadius = 0.6f; // the largest blur radius, in percent of the view's height (0.6 = 6.5 px at 1080 lines)

		// Vignette, applied by the tonemap after the roll-off. The Editor view leaves it out unless Effects > Post-Processing
		// is on.
		float VignetteIntensity = 0.0f; // how dark the corners get (0 = none, 1 = black)
		float VignetteRadius = 0.55f;   // where the darkening starts: 0 = the centre, 1 = the corners
		float VignetteSoftness = 0.5f;  // over how much further it reaches its full darkness

		// Colour grading in the Tonemap pass (#137); the defaults change nothing. White balance and contrast act before the
		// roll-off (contrast in log space, so it never clips); lift, gamma, gain and saturation after it.
		bool Grading = true;      // a quick off switch for comparisons
		float Temperature = 0.0f; // -1 (cooler, bluer) .. 1 (warmer, more orange)
		float Tint = 0.0f;        // -1 (greener) .. 1 (more magenta)
		float Contrast = 1.0f;    // 1 = as is; above 1 darker darks and brighter lights around middle grey
		float Saturation = 1.0f;  // 0 = grey, 1 = as is, 2 = twice as colourful
		glm::vec3 Lift{ 0.0f };   // added to the darks (fades out towards white): -0.2 .. 0.2 per channel
		glm::vec3 Gamma{ 1.0f };  // the mid-tones: above 1 brighter, below 1 darker, per channel
		glm::vec3 Gain{ 1.0f };   // multiplies everything (mostly the lights), per channel

		bool operator==(const ScenePostProcess& other) const
		{
			return Exposure == other.Exposure && Bloom == other.Bloom && BloomThreshold == other.BloomThreshold
				&& BloomKnee == other.BloomKnee && BloomIntensity == other.BloomIntensity && BloomRadius == other.BloomRadius
				&& DepthOfField == other.DepthOfField && DofNearStart == other.DofNearStart && DofFarStart == other.DofFarStart
				&& DofRange == other.DofRange && DofMaxRadius == other.DofMaxRadius && VignetteIntensity == other.VignetteIntensity
				&& VignetteRadius == other.VignetteRadius && VignetteSoftness == other.VignetteSoftness
				&& Grading == other.Grading && Temperature == other.Temperature && Tint == other.Tint && Contrast == other.Contrast
				&& Saturation == other.Saturation && Lift == other.Lift && Gamma == other.Gamma && Gain == other.Gain;
		}
		bool operator!=(const ScenePostProcess& other) const { return (*this == other) == false; }
	};
}
