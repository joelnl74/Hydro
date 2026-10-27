#pragma once
#include <cstdint>
#include <string>

namespace Hydro
{
	// Which render features a view uses (3D renderer MVP plan, "Render features and presets"). The engine
	// has no rendering modes named after a kind of game: a 2D game, a 3D game and pixel-art sprites in a
	// 3D world are combinations of these settings. The project's default lives in RendererSettings::Features
	// (saved in the .hyproj); a scene may override it (SceneRenderSettings).

	// Depth shared by sprites and meshes.
	enum class DepthMode : uint8_t
	{
		Off = 0,
		Shared
	};

	// How translucent things are ordered.
	enum class SpriteSortMode : uint8_t
	{
		SortOrder = 0, // SortOrder, then Z, then standing sprites far to near (the 2D order)
		ViewDepth      // by distance to the camera; SortOrder only breaks ties (3D)
	};

	enum class LightingMode : uint8_t
	{
		None = 0,
		LightBuffer2D, // 2D lights drawn into a light buffer multiplied over the view
		Forward        // directional, point and spot lights in the sprite and mesh shaders
	};

	// How the sprites submitted next are drawn in a view with shared depth (Renderer2D::SetSpriteDrawMode).
	// Ignored, and everything drawn Translucent, without shared depth, so the 2D preset is unchanged.
	enum class SpriteDrawMode : uint8_t
	{
		Translucent = 0, // depth test, no depth write, alpha blended, after the cut-out sprites (particles, effects)
		Decal,           // the same, but before the cut-out sprites: ground, roads, flat decals in painter's order
		Cutout           // depth test and write; pixels with alpha under 0.5 are discarded (standing sprites)
	};

	// The alpha mode of sprites that don't choose one.
	enum class SpriteAlphaDefault : uint8_t
	{
		Translucent = 0,
		Cutout
	};

	// Cast shadows (Roadmap R14, Diorama RPG H6). Needs forward lighting and shared depth.
	enum class ShadowMode : uint8_t
	{
		Off = 0,
		Sun // one shadow map for the main directional light, fitted around what the camera looks at
	};

	// How cut-out standing sprites (Upright, Billboard) cast sun shadows. Every lit sprite receives them.
	enum class SpriteShadowMode : uint8_t
	{
		None = 0, // sprites cast no shadow (meshes still do)
		AsDrawn,  // the quad as the camera sees it: cheap, but thin when the sun comes from the side
		FaceSun   // the quad turned about its feet to face the sun, so its shadow is always as wide as the sprite
	};

	struct RenderFeatureSettings
	{
		DepthMode Depth = DepthMode::Off;
		SpriteSortMode SpriteSorting = SpriteSortMode::SortOrder;
		LightingMode Lighting = LightingMode::LightBuffer2D;
		SpriteAlphaDefault DefaultSpriteAlpha = SpriteAlphaDefault::Translucent;
		bool Meshes = false; // MeshComponents are drawn; needs Depth = Shared
		ShadowMode Shadows = ShadowMode::Off; // needs Lighting = Forward and Depth = Shared
		SpriteShadowMode SpriteShadows = SpriteShadowMode::FaceSun; // only read with Shadows on
		// HDR scene colour (post-processing plan P1): the scene renders into RGBA16F and a Tonemap pass (exposure,
		// PBR Neutral) writes the view's sRGB colour; the overlay and debug lines are drawn after it.
		bool HDR = false;
		// Bloom (post-processing plan P2): a soft glow around what is brighter than the scene's threshold, added
		// to the HDR scene colour before the tonemap. Needs HDR.
		bool Bloom = false;

		bool operator==(const RenderFeatureSettings& other) const
		{
			return Depth == other.Depth && SpriteSorting == other.SpriteSorting && Lighting == other.Lighting
				&& DefaultSpriteAlpha == other.DefaultSpriteAlpha && Meshes == other.Meshes
				&& Shadows == other.Shadows && SpriteShadows == other.SpriteShadows && HDR == other.HDR && Bloom == other.Bloom;
		}
		bool operator!=(const RenderFeatureSettings& other) const { return (*this == other) == false; }

		// Today's frame: painter's order, the 2D light buffer, no depth.
		static RenderFeatureSettings Preset2D() { return {}; }
		// Shared depth, view-depth sorting, forward lights, cut-out sprites, meshes, sun shadows with
		// sun-facing sprite shadows, HDR with the Neutral tonemap, bloom.
		static RenderFeatureSettings Preset3D()
		{
			RenderFeatureSettings settings;
			settings.Depth = DepthMode::Shared;
			settings.SpriteSorting = SpriteSortMode::ViewDepth;
			settings.Lighting = LightingMode::Forward;
			settings.DefaultSpriteAlpha = SpriteAlphaDefault::Cutout;
			settings.Meshes = true;
			settings.Shadows = ShadowMode::Sun;
			settings.SpriteShadows = SpriteShadowMode::FaceSun;
			settings.HDR = true;
			settings.Bloom = true;
			return settings;
		}
		// "2D", "3D" or "Custom".
		const char* GetPresetName() const
		{
			if (*this == Preset2D())
			{
				return "2D";
			}
			if (*this == Preset3D())
			{
				return "3D";
			}
			return "Custom";
		}
	};

	// A scene's rendering choice, saved in the scene and copied on Play.
	struct SceneRenderSettings
	{
		// false: the scene uses the project's RendererSettings::Features.
		bool OverrideProject = false;
		RenderFeatureSettings Features;
	};

	enum class RenderValidationSeverity : uint8_t
	{
		Warning = 0, // the feature is kept
		Error        // the feature is left out; the frame still draws
	};

	// A problem with a feature, its settings or the GPU, reported like a validation-layer message.
	struct RenderValidationMessage
	{
		RenderValidationSeverity Severity = RenderValidationSeverity::Warning;
		std::string Feature;
		std::string Text;
	};
}
