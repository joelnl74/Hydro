#include "hypch.h"
#include "RenderFeatureSerializer.h"

namespace Hydro
{
	static const char* DepthToString(DepthMode mode) { return mode == DepthMode::Shared ? "Shared" : "Off"; }
	static const char* SortToString(SpriteSortMode mode) { return mode == SpriteSortMode::ViewDepth ? "ViewDepth" : "SortOrder"; }
	static const char* AlphaToString(SpriteAlphaDefault mode) { return mode == SpriteAlphaDefault::Cutout ? "Cutout" : "Translucent"; }
	static const char* ShadowsToString(ShadowMode mode) { return mode == ShadowMode::Sun ? "Sun" : "Off"; }
	static const char* SpriteShadowsToString(SpriteShadowMode mode)
	{
		switch (mode)
		{
			case SpriteShadowMode::None:    return "None";
			case SpriteShadowMode::AsDrawn: return "AsDrawn";
			default:                        return "FaceSun";
		}
	}
	static const char* LightingToString(LightingMode mode)
	{
		switch (mode)
		{
			case LightingMode::None:    return "None";
			case LightingMode::Forward: return "Forward";
			default:                    return "LightBuffer2D";
		}
	}

	void RenderFeatureSerializer::Serialize(YAML::Emitter& out, const RenderFeatureSettings& settings)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Depth" << YAML::Value << DepthToString(settings.Depth);
		out << YAML::Key << "SpriteSorting" << YAML::Value << SortToString(settings.SpriteSorting);
		out << YAML::Key << "Lighting" << YAML::Value << LightingToString(settings.Lighting);
		out << YAML::Key << "DefaultSpriteAlpha" << YAML::Value << AlphaToString(settings.DefaultSpriteAlpha);
		out << YAML::Key << "Meshes" << YAML::Value << settings.Meshes;
		out << YAML::Key << "Shadows" << YAML::Value << ShadowsToString(settings.Shadows);
		out << YAML::Key << "SpriteShadows" << YAML::Value << SpriteShadowsToString(settings.SpriteShadows);
		out << YAML::Key << "HDR" << YAML::Value << settings.HDR;
		out << YAML::Key << "Bloom" << YAML::Value << settings.Bloom;
		out << YAML::EndMap;
	}

	void RenderFeatureSerializer::Deserialize(const YAML::Node& node, RenderFeatureSettings& settings)
	{
		if (node.IsMap() == false)
		{
			return;
		}

		if (auto value = node["Depth"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "Shared")
			{
				settings.Depth = DepthMode::Shared;
			}
			else if (name == "Off")
			{
				settings.Depth = DepthMode::Off;
			}
		}
		if (auto value = node["SpriteSorting"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "ViewDepth")
			{
				settings.SpriteSorting = SpriteSortMode::ViewDepth;
			}
			else if (name == "SortOrder")
			{
				settings.SpriteSorting = SpriteSortMode::SortOrder;
			}
		}
		if (auto value = node["Lighting"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "None")
			{
				settings.Lighting = LightingMode::None;
			}
			else if (name == "LightBuffer2D")
			{
				settings.Lighting = LightingMode::LightBuffer2D;
			}
			else if (name == "Forward")
			{
				settings.Lighting = LightingMode::Forward;
			}
		}
		if (auto value = node["DefaultSpriteAlpha"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "Cutout")
			{
				settings.DefaultSpriteAlpha = SpriteAlphaDefault::Cutout;
			}
			else if (name == "Translucent")
			{
				settings.DefaultSpriteAlpha = SpriteAlphaDefault::Translucent;
			}
		}
		if (auto value = node["Meshes"])
		{
			settings.Meshes = value.as<bool>(settings.Meshes);
		}
		// Settings saved before sun shadows (R14) have neither key: shadows stay off.
		if (auto value = node["Shadows"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "Sun")
			{
				settings.Shadows = ShadowMode::Sun;
			}
			else if (name == "Off")
			{
				settings.Shadows = ShadowMode::Off;
			}
		}
		// Settings saved before HDR (P1) have no key: the 8-bit path, as before.
		if (auto value = node["HDR"])
		{
			settings.HDR = value.as<bool>(settings.HDR);
		}
		if (auto value = node["Bloom"])
		{
			settings.Bloom = value.as<bool>(settings.Bloom);
		}
		if (auto value = node["SpriteShadows"])
		{
			const std::string name = value.as<std::string>("");
			if (name == "None")
			{
				settings.SpriteShadows = SpriteShadowMode::None;
			}
			else if (name == "AsDrawn")
			{
				settings.SpriteShadows = SpriteShadowMode::AsDrawn;
			}
			else if (name == "FaceSun")
			{
				settings.SpriteShadows = SpriteShadowMode::FaceSun;
			}
		}
	}
}
