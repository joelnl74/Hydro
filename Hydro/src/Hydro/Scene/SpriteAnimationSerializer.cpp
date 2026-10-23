#include "hypch.h"
#include "SpriteAnimationSerializer.h"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>

namespace Hydro
{
	static constexpr float DefaultFrameRate = 10.0f;

	static const char* LoopModeToString(AnimationLoopMode mode)
	{
		switch (mode)
		{
			case AnimationLoopMode::Once: return "Once";
			case AnimationLoopMode::PingPong: return "PingPong";
			case AnimationLoopMode::Loop: return "Loop";
		}
		return "Loop";
	}

	static AnimationLoopMode LoopModeFromString(const std::string& value)
	{
		if (value == "Once")
		{
			return AnimationLoopMode::Once;
		}
		if (value == "PingPong")
		{
			return AnimationLoopMode::PingPong;
		}
		return AnimationLoopMode::Loop;
	}

	// A frame's sprite by name, or by index when it is a plain number. -1 when it matches neither.
	static int32_t ResolveSprite(const Texture2D& texture, const std::string& value)
	{
		const int32_t byName = texture.FindSprite(value);
		if (byName >= 0)
		{
			return byName;
		}

		if (value.empty() == false && std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }))
		{
			const unsigned long index = std::stoul(value);
			if (index < texture.GetSpriteCount())
			{
				return (int32_t)index;
			}
		}
		return -1;
	}

	// The texture's path as written in the file: relative to the .hyanim's folder when possible.
	static std::string ToSavedTexturePath(const std::filesystem::path& texturePath, const std::filesystem::path& animationFile)
	{
		std::error_code error;
		const std::filesystem::path relative = std::filesystem::relative(texturePath, animationFile.parent_path(), error);
		if (error.value() == 0 && relative.empty() == false)
		{
			return relative.generic_u8string();
		}
		return texturePath.generic_u8string();
	}

	bool SpriteAnimationSerializer::Serialize(const std::filesystem::path& filepath, const SpriteAnimationSet& set)
	{
		const Ref<Texture2D>& texture = set.GetTexture();
		if (texture == nullptr || texture->GetPath().empty())
		{
			HY_CORE_ERROR("SpriteAnimationSerializer: '{}': the set's texture has no file, so it can't be saved", filepath.string());
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "SpriteAnimationSet" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Texture" << YAML::Value << ToSavedTexturePath(texture->GetPath(), filepath);
		if (set.GetClipCount() > 0)
		{
			out << YAML::Key << "DefaultClip" << YAML::Value << set.GetClip(set.GetDefaultClip()).Name;
		}

		out << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
		for (uint32_t i = 0; i < set.GetClipCount(); i++)
		{
			const SpriteAnimationClip& clip = set.GetClip(i);
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << clip.Name;
			out << YAML::Key << "Loop" << YAML::Value << LoopModeToString(clip.Loop);
			if (clip.FlipX)
			{
				out << YAML::Key << "FlipX" << YAML::Value << true;
			}

			// The short form when every frame lasts the same.
			const bool evenFrames = clip.Frames.empty() == false && std::all_of(clip.Frames.begin(), clip.Frames.end(),
				[&](const SpriteAnimationFrame& frame) { return std::abs(frame.Duration - clip.Frames[0].Duration) < 1e-6f; });
			if (evenFrames)
			{
				out << YAML::Key << "FrameRate" << YAML::Value << 1.0f / clip.Frames[0].Duration;
				out << YAML::Key << "Frames" << YAML::Value << YAML::Flow << YAML::BeginSeq;
				for (const SpriteAnimationFrame& frame : clip.Frames)
				{
					out << texture->GetSprite(frame.Sprite).Name;
				}
				out << YAML::EndSeq;
			}
			else
			{
				out << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
				for (const SpriteAnimationFrame& frame : clip.Frames)
				{
					out << YAML::Flow << YAML::BeginMap;
					out << YAML::Key << "Sprite" << YAML::Value << texture->GetSprite(frame.Sprite).Name;
					out << YAML::Key << "Duration" << YAML::Value << frame.Duration;
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			if (clip.Events.empty() == false)
			{
				out << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;
				for (const SpriteAnimationEvent& event : clip.Events)
				{
					out << YAML::Flow << YAML::BeginMap;
					out << YAML::Key << "Frame" << YAML::Value << event.Frame;
					out << YAML::Key << "Name" << YAML::Value << event.Name;
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream file(filepath);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("SpriteAnimationSerializer: can't write '{}'", filepath.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	bool SpriteAnimationSerializer::Deserialize(const std::filesystem::path& filepath, SpriteAnimationSetSpecification& specification)
	{
		specification = {};

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("SpriteAnimationSerializer: can't read '{}': {}", filepath.string(), e.what());
			return false;
		}

		const YAML::Node root = data["SpriteAnimationSet"];
		if (root.IsDefined() == false || root.IsMap() == false)
		{
			HY_CORE_ERROR("SpriteAnimationSerializer: '{}' has no SpriteAnimationSet section", filepath.string());
			return false;
		}

		try
		{
			std::string texturePath;
			if (auto node = root["Texture"])
			{
				texturePath = node.as<std::string>("");
			}
			if (texturePath.empty())
			{
				HY_CORE_ERROR("SpriteAnimationSerializer: '{}' names no Texture", filepath.string());
				return false;
			}

			std::filesystem::path textureFile = std::filesystem::u8path(texturePath);
			if (textureFile.is_relative())
			{
				textureFile = filepath.parent_path() / textureFile;
			}
			specification.Texture = Texture2D::LoadFromFile(textureFile);
			if (specification.Texture == nullptr)
			{
				// LoadFromFile logged why.
				return false;
			}
			const Texture2D& texture = *specification.Texture;

			std::string defaultClip;
			if (auto node = root["DefaultClip"])
			{
				defaultClip = node.as<std::string>("");
			}

			if (auto clipsNode = root["Clips"])
			{
				for (const auto& clipNode : clipsNode)
				{
					SpriteAnimationClip clip;
					if (auto node = clipNode["Name"])
					{
						clip.Name = node.as<std::string>("");
					}
					if (auto node = clipNode["Loop"])
					{
						clip.Loop = LoopModeFromString(node.as<std::string>(""));
					}
					if (auto node = clipNode["FlipX"])
					{
						clip.FlipX = node.as<bool>(false);
					}

					float frameRate = DefaultFrameRate;
					if (auto node = clipNode["FrameRate"])
					{
						frameRate = node.as<float>(DefaultFrameRate);
					}
					const float evenDuration = 1.0f / std::max(frameRate, 0.001f);

					if (auto framesNode = clipNode["Frames"])
					{
						for (const auto& frameNode : framesNode)
						{
							std::string spriteName;
							float duration = evenDuration;
							if (frameNode.IsScalar())
							{
								spriteName = frameNode.as<std::string>("");
							}
							else
							{
								if (auto node = frameNode["Sprite"])
								{
									spriteName = node.as<std::string>("");
								}
								if (auto node = frameNode["Duration"])
								{
									duration = node.as<float>(evenDuration);
								}
							}

							const int32_t sprite = ResolveSprite(texture, spriteName);
							if (sprite < 0)
							{
								HY_CORE_WARN("SpriteAnimationSerializer: '{}': clip '{}': '{}' has no sprite '{}'; frame left out",
									filepath.string(), clip.Name, texture.GetPath().filename().string(), spriteName);
								continue;
							}
							clip.Frames.push_back({ (uint32_t)sprite, duration });
						}
					}

					if (auto eventsNode = clipNode["Events"])
					{
						for (const auto& eventNode : eventsNode)
						{
							SpriteAnimationEvent event;
							if (auto node = eventNode["Frame"])
							{
								event.Frame = node.as<uint32_t>(0);
							}
							if (auto node = eventNode["Name"])
							{
								event.Name = node.as<std::string>("");
							}
							clip.Events.push_back(std::move(event));
						}
					}

					if (clip.Name == defaultClip)
					{
						specification.DefaultClip = (uint32_t)specification.Clips.size();
					}
					specification.Clips.push_back(std::move(clip));
				}
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("SpriteAnimationSerializer: '{}' is malformed: {}", filepath.string(), e.what());
			specification = {};
			return false;
		}

		return true;
	}
}
