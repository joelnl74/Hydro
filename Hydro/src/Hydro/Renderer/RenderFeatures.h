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

	// The alpha mode of sprites that don't choose one.
	enum class SpriteAlphaDefault : uint8_t
	{
		Translucent = 0,
		Cutout
	};

	struct RenderFeatureSettings
	{
		DepthMode Depth = DepthMode::Off;
		SpriteSortMode SpriteSorting = SpriteSortMode::SortOrder;
		LightingMode Lighting = LightingMode::LightBuffer2D;
		SpriteAlphaDefault DefaultSpriteAlpha = SpriteAlphaDefault::Translucent;
		bool Meshes = false; // MeshComponents are drawn; needs Depth = Shared

		bool operator==(const RenderFeatureSettings& other) const
		{
			return Depth == other.Depth && SpriteSorting == other.SpriteSorting && Lighting == other.Lighting
				&& DefaultSpriteAlpha == other.DefaultSpriteAlpha && Meshes == other.Meshes;
		}
		bool operator!=(const RenderFeatureSettings& other) const { return (*this == other) == false; }

		// Today's frame: painter's order, the 2D light buffer, no depth.
		static RenderFeatureSettings Preset2D() { return {}; }
		// Shared depth, view-depth sorting, forward lights, cut-out sprites, meshes.
		static RenderFeatureSettings Preset3D()
		{
			RenderFeatureSettings settings;
			settings.Depth = DepthMode::Shared;
			settings.SpriteSorting = SpriteSortMode::ViewDepth;
			settings.Lighting = LightingMode::Forward;
			settings.DefaultSpriteAlpha = SpriteAlphaDefault::Cutout;
			settings.Meshes = true;
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
