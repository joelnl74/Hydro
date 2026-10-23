#include "hypch.h"
#include "TextureImportSerializer.h"

#include "Hydro/Utils/YAMLConversions.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	static const char* ImageFormatToString(ImageFormat format)
	{
		return format == ImageFormat::RGBA ? "RGBA" : "SRGB";
	}

	static ImageFormat ImageFormatFromString(const std::string& value)
	{
		return value == "RGBA" ? ImageFormat::RGBA : ImageFormat::SRGB;
	}

	static const char* FilterToString(TextureFilter filter)
	{
		return filter == TextureFilter::Nearest ? "Nearest" : "Linear";
	}

	static TextureFilter FilterFromString(const std::string& value)
	{
		return value == "Nearest" ? TextureFilter::Nearest : TextureFilter::Linear;
	}

	static const char* WrapToString(TextureWrap wrap)
	{
		return wrap == TextureWrap::Clamp ? "Clamp" : "Repeat";
	}

	static TextureWrap WrapFromString(const std::string& value)
	{
		return value == "Clamp" ? TextureWrap::Clamp : TextureWrap::Repeat;
	}

	static const char* SpriteModeToString(SpriteMode mode)
	{
		return mode == SpriteMode::Multiple ? "Multiple" : "Single";
	}

	static SpriteMode SpriteModeFromString(const std::string& value)
	{
		return value == "Multiple" ? SpriteMode::Multiple : SpriteMode::Single;
	}

	static const char* SlicingModeToString(SpriteSlicingMode mode)
	{
		switch (mode)
		{
			case SpriteSlicingMode::Manual: return "Manual";
			case SpriteSlicingMode::Auto: return "Auto";
			case SpriteSlicingMode::Grid: return "Grid";
		}
		return "Grid";
	}

	static SpriteSlicingMode SlicingModeFromString(const std::string& value)
	{
		if (value == "Manual")
		{
			return SpriteSlicingMode::Manual;
		}
		if (value == "Auto")
		{
			return SpriteSlicingMode::Auto;
		}
		return SpriteSlicingMode::Grid;
	}

	bool TextureImportSerializer::Serialize(const std::filesystem::path& filepath, const TextureImportConfig& config)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "TextureImport" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "Format" << YAML::Value << ImageFormatToString(config.Format);
		out << YAML::Key << "Filter" << YAML::Value << FilterToString(config.Properties.SamplerFilter);
		out << YAML::Key << "Wrap" << YAML::Value << WrapToString(config.Properties.SamplerWrap);
		if (config.UseColorKey)
		{
			out << YAML::Key << "ColorKey" << YAML::Value << config.ColorKey;
		}
		out << YAML::Key << "SpriteMode" << YAML::Value << SpriteModeToString(config.Mode);
		out << YAML::Key << "PixelsPerUnit" << YAML::Value << config.PixelsPerUnit;

		if (config.Mode == SpriteMode::Multiple)
		{
			const SpriteSlicingConfig& slicing = config.Slicing;
			out << YAML::Key << "Slicing" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Mode" << YAML::Value << SlicingModeToString(slicing.Mode);
			out << YAML::Key << "CellSize" << YAML::Value << slicing.CellSize;
			out << YAML::Key << "Offset" << YAML::Value << slicing.Offset;
			out << YAML::Key << "Spacing" << YAML::Value << slicing.Spacing;
			out << YAML::Key << "Columns" << YAML::Value << slicing.Columns;
			out << YAML::Key << "Rows" << YAML::Value << slicing.Rows;
			out << YAML::Key << "RowNames" << YAML::Value << YAML::Flow << slicing.RowNames;
			out << YAML::Key << "DefaultPivot" << YAML::Value << slicing.DefaultPivot;
			out << YAML::EndMap;

			out << YAML::Key << "Sprites" << YAML::Value << YAML::BeginSeq;
			for (const SpriteRegion& sprite : config.Sprites)
			{
				out << YAML::Flow << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << sprite.Name;
				out << YAML::Key << "Rect" << YAML::Value << YAML::Flow << YAML::BeginSeq
					<< sprite.Position.x << sprite.Position.y << sprite.Size.x << sprite.Size.y << YAML::EndSeq;
				out << YAML::Key << "Pivot" << YAML::Value << sprite.Pivot;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream file(filepath);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("TextureImportSerializer: can't write '{}'", filepath.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	bool TextureImportSerializer::Deserialize(const std::filesystem::path& filepath, TextureImportConfig& config)
	{
		config = {};

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("TextureImportSerializer: can't read '{}': {}", filepath.string(), e.what());
			return false;
		}

		const YAML::Node root = data["TextureImport"];
		if (root.IsDefined() == false || root.IsMap() == false)
		{
			HY_CORE_ERROR("TextureImportSerializer: '{}' has no TextureImport section", filepath.string());
			return false;
		}

		try
		{
			if (auto node = root["Format"])
			{
				config.Format = ImageFormatFromString(node.as<std::string>(""));
			}
			if (auto node = root["Filter"])
			{
				config.Properties.SamplerFilter = FilterFromString(node.as<std::string>(""));
			}
			if (auto node = root["Wrap"])
			{
				config.Properties.SamplerWrap = WrapFromString(node.as<std::string>(""));
			}
			if (auto node = root["ColorKey"])
			{
				config.UseColorKey = true;
				config.ColorKey = glm::min(node.as<glm::uvec3>(config.ColorKey), glm::uvec3(255));
			}
			if (auto node = root["SpriteMode"])
			{
				config.Mode = SpriteModeFromString(node.as<std::string>(""));
			}
			if (auto node = root["PixelsPerUnit"])
			{
				config.PixelsPerUnit = std::max(node.as<float>(config.PixelsPerUnit), 0.001f);
			}

			if (auto slicingNode = root["Slicing"])
			{
				SpriteSlicingConfig& slicing = config.Slicing;
				if (auto node = slicingNode["Mode"])
				{
					slicing.Mode = SlicingModeFromString(node.as<std::string>(""));
				}
				if (auto node = slicingNode["CellSize"])
				{
					slicing.CellSize = glm::max(node.as<glm::uvec2>(slicing.CellSize), glm::uvec2(1));
				}
				if (auto node = slicingNode["Offset"])
				{
					slicing.Offset = node.as<glm::uvec2>(slicing.Offset);
				}
				if (auto node = slicingNode["Spacing"])
				{
					slicing.Spacing = node.as<glm::uvec2>(slicing.Spacing);
				}
				if (auto node = slicingNode["Columns"])
				{
					slicing.Columns = node.as<uint32_t>(slicing.Columns);
				}
				if (auto node = slicingNode["Rows"])
				{
					slicing.Rows = node.as<uint32_t>(slicing.Rows);
				}
				if (auto node = slicingNode["RowNames"])
				{
					for (const auto& nameNode : node)
					{
						slicing.RowNames.push_back(nameNode.as<std::string>(""));
					}
				}
				if (auto node = slicingNode["DefaultPivot"])
				{
					slicing.DefaultPivot = node.as<glm::vec2>(slicing.DefaultPivot);
				}
			}

			if (auto spritesNode = root["Sprites"])
			{
				for (const auto& spriteNode : spritesNode)
				{
					SpriteRegion sprite;
					if (auto node = spriteNode["Name"])
					{
						sprite.Name = node.as<std::string>("");
					}
					if (auto node = spriteNode["Rect"])
					{
						if (node.IsSequence() && node.size() == 4)
						{
							sprite.Position = { node[0].as<uint32_t>(0), node[1].as<uint32_t>(0) };
							sprite.Size = { node[2].as<uint32_t>(0), node[3].as<uint32_t>(0) };
						}
					}
					if (auto node = spriteNode["Pivot"])
					{
						sprite.Pivot = node.as<glm::vec2>(sprite.Pivot);
					}

					if (sprite.Size.x == 0 || sprite.Size.y == 0)
					{
						HY_CORE_WARN("TextureImportSerializer: '{}': sprite '{}' has no size; skipped", filepath.string(), sprite.Name);
						continue;
					}
					config.Sprites.push_back(std::move(sprite));
				}
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("TextureImportSerializer: '{}' is malformed: {}", filepath.string(), e.what());
			config = {};
			return false;
		}

		return true;
	}
}
