#include "hypch.h"
#include "RenderProfileSerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	void RenderProfileSerializer::SerializeEnvironment(YAML::Emitter& out, const SceneEnvironment& environment)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "SkyAmbient" << YAML::Value << environment.SkyAmbient;
		out << YAML::Key << "GroundAmbient" << YAML::Value << environment.GroundAmbient;
		out << YAML::Key << "Fog" << YAML::Value << environment.Fog;
		out << YAML::Key << "FogColor" << YAML::Value << environment.FogColor;
		out << YAML::Key << "FogStart" << YAML::Value << environment.FogStart;
		out << YAML::Key << "FogEnd" << YAML::Value << environment.FogEnd;
		out << YAML::EndMap;
	}

	SceneEnvironment RenderProfileSerializer::DeserializeEnvironment(const YAML::Node& node)
	{
		SceneEnvironment environment;
		if (node.IsDefined() == false || node.IsMap() == false)
		{
			return environment;
		}

		environment.Override = true;
		environment.SkyAmbient = node["SkyAmbient"].as<glm::vec3>(environment.SkyAmbient);
		environment.GroundAmbient = node["GroundAmbient"].as<glm::vec3>(environment.GroundAmbient);
		environment.Fog = node["Fog"].as<bool>(environment.Fog);
		environment.FogColor = node["FogColor"].as<glm::vec3>(environment.FogColor);
		environment.FogStart = node["FogStart"].as<float>(environment.FogStart);
		environment.FogEnd = node["FogEnd"].as<float>(environment.FogEnd);
		return environment;
	}

	void RenderProfileSerializer::SerializePostProcess(YAML::Emitter& out, const ScenePostProcess& postProcess)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Exposure" << YAML::Value << postProcess.Exposure;
		out << YAML::Key << "Bloom" << YAML::Value << postProcess.Bloom;
		out << YAML::Key << "BloomThreshold" << YAML::Value << postProcess.BloomThreshold;
		out << YAML::Key << "BloomKnee" << YAML::Value << postProcess.BloomKnee;
		out << YAML::Key << "BloomIntensity" << YAML::Value << postProcess.BloomIntensity;
		out << YAML::Key << "BloomRadius" << YAML::Value << postProcess.BloomRadius;
		out << YAML::Key << "DepthOfField" << YAML::Value << postProcess.DepthOfField;
		out << YAML::Key << "DofNearStart" << YAML::Value << postProcess.DofNearStart;
		out << YAML::Key << "DofFarStart" << YAML::Value << postProcess.DofFarStart;
		out << YAML::Key << "DofRange" << YAML::Value << postProcess.DofRange;
		out << YAML::Key << "DofMaxRadius" << YAML::Value << postProcess.DofMaxRadius;
		out << YAML::Key << "VignetteIntensity" << YAML::Value << postProcess.VignetteIntensity;
		out << YAML::Key << "VignetteRadius" << YAML::Value << postProcess.VignetteRadius;
		out << YAML::Key << "VignetteSoftness" << YAML::Value << postProcess.VignetteSoftness;
		out << YAML::Key << "Grading" << YAML::Value << postProcess.Grading;
		out << YAML::Key << "Temperature" << YAML::Value << postProcess.Temperature;
		out << YAML::Key << "Tint" << YAML::Value << postProcess.Tint;
		out << YAML::Key << "Contrast" << YAML::Value << postProcess.Contrast;
		out << YAML::Key << "Saturation" << YAML::Value << postProcess.Saturation;
		out << YAML::Key << "Lift" << YAML::Value << postProcess.Lift;
		out << YAML::Key << "Gamma" << YAML::Value << postProcess.Gamma;
		out << YAML::Key << "Gain" << YAML::Value << postProcess.Gain;
		out << YAML::EndMap;
	}

	ScenePostProcess RenderProfileSerializer::DeserializePostProcess(const YAML::Node& node)
	{
		ScenePostProcess postProcess;
		if (node.IsDefined() == false || node.IsMap() == false)
		{
			return postProcess;
		}

		if (auto exposure = node["Exposure"])
		{
			postProcess.Exposure = exposure.as<float>(postProcess.Exposure);
		}
		if (auto bloom = node["Bloom"])
		{
			postProcess.Bloom = bloom.as<bool>(postProcess.Bloom);
		}
		if (auto threshold = node["BloomThreshold"])
		{
			postProcess.BloomThreshold = threshold.as<float>(postProcess.BloomThreshold);
		}
		if (auto knee = node["BloomKnee"])
		{
			postProcess.BloomKnee = knee.as<float>(postProcess.BloomKnee);
		}
		if (auto intensity = node["BloomIntensity"])
		{
			postProcess.BloomIntensity = intensity.as<float>(postProcess.BloomIntensity);
		}
		if (auto radius = node["BloomRadius"])
		{
			postProcess.BloomRadius = radius.as<float>(postProcess.BloomRadius);
		}
		// Saved before depth of field (#135): no keys, the defaults.
		if (auto depthOfField = node["DepthOfField"])
		{
			postProcess.DepthOfField = depthOfField.as<bool>(postProcess.DepthOfField);
		}
		if (auto nearStart = node["DofNearStart"])
		{
			postProcess.DofNearStart = nearStart.as<float>(postProcess.DofNearStart);
		}
		if (auto farStart = node["DofFarStart"])
		{
			postProcess.DofFarStart = farStart.as<float>(postProcess.DofFarStart);
		}
		if (auto range = node["DofRange"])
		{
			postProcess.DofRange = range.as<float>(postProcess.DofRange);
		}
		if (auto maxRadius = node["DofMaxRadius"])
		{
			postProcess.DofMaxRadius = maxRadius.as<float>(postProcess.DofMaxRadius);
		}
		// Saved before the vignette (Nov 3, 2026): no keys, none.
		if (auto vignette = node["VignetteIntensity"])
		{
			postProcess.VignetteIntensity = vignette.as<float>(postProcess.VignetteIntensity);
		}
		if (auto vignetteRadius = node["VignetteRadius"])
		{
			postProcess.VignetteRadius = vignetteRadius.as<float>(postProcess.VignetteRadius);
		}
		if (auto vignetteSoftness = node["VignetteSoftness"])
		{
			postProcess.VignetteSoftness = vignetteSoftness.as<float>(postProcess.VignetteSoftness);
		}
		// Saved before colour grading (#137, Nov 3, 2026): no keys, the defaults (which change nothing).
		postProcess.Grading = node["Grading"].as<bool>(postProcess.Grading);
		postProcess.Temperature = node["Temperature"].as<float>(postProcess.Temperature);
		postProcess.Tint = node["Tint"].as<float>(postProcess.Tint);
		postProcess.Contrast = node["Contrast"].as<float>(postProcess.Contrast);
		postProcess.Saturation = node["Saturation"].as<float>(postProcess.Saturation);
		postProcess.Lift = node["Lift"].as<glm::vec3>(postProcess.Lift);
		postProcess.Gamma = node["Gamma"].as<glm::vec3>(postProcess.Gamma);
		postProcess.Gain = node["Gain"].as<glm::vec3>(postProcess.Gain);
		return postProcess;
	}

	const char* RenderProfileSerializer::LookSourceToString(RenderLookSource source)
	{
		switch (source)
		{
			case RenderLookSource::Profile: return "Profile";
			case RenderLookSource::Scene: return "Scene";
		}
		return "Scene";
	}

	RenderLookSource RenderProfileSerializer::LookSourceFromString(const std::string& name)
	{
		return name == "Profile" ? RenderLookSource::Profile : RenderLookSource::Scene;
	}

	bool RenderProfileSerializer::Serialize(const std::filesystem::path& filepath, const RenderProfileData& data)
	{
		// The profile's own handle goes first, like in scenes; a new file gets one here.
		const AssetHandle handle = AssetManager::ImportAsset(filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		if ((uint64_t)handle != 0)
		{
			out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		}
		out << YAML::Key << "RenderProfile" << YAML::Value << YAML::BeginMap;
		if (data.Environment.Override)
		{
			out << YAML::Key << "Environment" << YAML::Value;
			SerializeEnvironment(out, data.Environment);
		}
		out << YAML::Key << "PostProcess" << YAML::Value;
		SerializePostProcess(out, data.PostProcess);
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream file(filepath);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("RenderProfileSerializer: can't write '{}'", filepath.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	bool RenderProfileSerializer::Deserialize(const std::filesystem::path& filepath, RenderProfileData& data)
	{
		data = {};

		YAML::Node root;
		try
		{
			root = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("RenderProfileSerializer: can't read '{}': {}", filepath.string(), e.what());
			return false;
		}

		const YAML::Node profile = root["RenderProfile"];
		if (profile.IsDefined() == false || profile.IsMap() == false)
		{
			HY_CORE_ERROR("RenderProfileSerializer: '{}' has no RenderProfile section", filepath.string());
			return false;
		}

		try
		{
			data.Environment = DeserializeEnvironment(profile["Environment"]);
			data.PostProcess = DeserializePostProcess(profile["PostProcess"]);
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("RenderProfileSerializer: bad data in '{}': {}", filepath.string(), e.what());
			data = {};
			return false;
		}
		return true;
	}
}
