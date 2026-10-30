#include "hypch.h"
#include "TerrainSerializer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"
#include "Hydro/Renderer/ImageWriter.h"
#include "Hydro/Utils/YAMLConversions.h"

#include <stb_image.h>
#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	static const char* DecorPlaceToString(TerrainDecorPlace place)
	{
		switch (place)
		{
			case TerrainDecorPlace::Layer: return "Layer";
			case TerrainDecorPlace::LayerBorder: return "LayerBorder";
			case TerrainDecorPlace::WallTop: return "WallTop";
			case TerrainDecorPlace::WallFoot: return "WallFoot";
		}
		return "Layer";
	}

	static TerrainDecorPlace DecorPlaceFromString(const std::string& text)
	{
		if (text == "LayerBorder")
		{
			return TerrainDecorPlace::LayerBorder;
		}
		if (text == "WallTop")
		{
			return TerrainDecorPlace::WallTop;
		}
		if (text == "WallFoot")
		{
			return TerrainDecorPlace::WallFoot;
		}
		return TerrainDecorPlace::Layer;
	}

	static const char* DecorShapeToString(TerrainDecorShape shape)
	{
		switch (shape)
		{
			case TerrainDecorShape::Upright: return "Upright";
			case TerrainDecorShape::Flat: return "Flat";
			case TerrainDecorShape::Stone: return "Stone";
		}
		return "Upright";
	}

	static TerrainDecorShape DecorShapeFromString(const std::string& text)
	{
		if (text == "Flat")
		{
			return TerrainDecorShape::Flat;
		}
		if (text == "Stone")
		{
			return TerrainDecorShape::Stone;
		}
		return TerrainDecorShape::Upright;
	}

	static const char* s_Base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string TerrainSerializer::EncodeBase64(const std::vector<uint8_t>& bytes, size_t lineLength)
	{
		std::string text;
		text.reserve(bytes.size() * 4 / 3 + bytes.size() / 57 + 4);
		size_t column = 0;
		const auto put = [&](char c)
		{
			if (lineLength > 0 && column == lineLength)
			{
				text.push_back('\n');
				column = 0;
			}
			text.push_back(c);
			column++;
		};

		size_t i = 0;
		for (; i + 2 < bytes.size(); i += 3)
		{
			const uint32_t triple = ((uint32_t)bytes[i] << 16) | ((uint32_t)bytes[i + 1] << 8) | bytes[i + 2];
			put(s_Base64Alphabet[(triple >> 18) & 63]);
			put(s_Base64Alphabet[(triple >> 12) & 63]);
			put(s_Base64Alphabet[(triple >> 6) & 63]);
			put(s_Base64Alphabet[triple & 63]);
		}
		const size_t rest = bytes.size() - i;
		if (rest > 0)
		{
			const uint32_t triple = ((uint32_t)bytes[i] << 16) | (rest > 1 ? ((uint32_t)bytes[i + 1] << 8) : 0u);
			put(s_Base64Alphabet[(triple >> 18) & 63]);
			put(s_Base64Alphabet[(triple >> 12) & 63]);
			put(rest > 1 ? s_Base64Alphabet[(triple >> 6) & 63] : '=');
			put('=');
		}
		return text;
	}

	static int Base64Value(char c)
	{
		if (c >= 'A' && c <= 'Z')
		{
			return c - 'A';
		}
		if (c >= 'a' && c <= 'z')
		{
			return c - 'a' + 26;
		}
		if (c >= '0' && c <= '9')
		{
			return c - '0' + 52;
		}
		if (c == '+')
		{
			return 62;
		}
		if (c == '/')
		{
			return 63;
		}
		return -1;
	}

	bool TerrainSerializer::DecodeBase64(const std::string& text, std::vector<uint8_t>& bytes)
	{
		bytes.clear();
		uint32_t buffer = 0;
		int bits = 0;
		for (char c : text)
		{
			if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t')
			{
				continue;
			}
			const int value = Base64Value(c);
			if (value < 0)
			{
				return false;
			}
			buffer = (buffer << 6) | (uint32_t)value;
			bits += 6;
			if (bits >= 8)
			{
				bits -= 8;
				bytes.push_back((uint8_t)((buffer >> bits) & 0xff));
			}
		}
		return true;
	}

	std::vector<uint8_t> TerrainSerializer::EncodeRunLength(const std::vector<uint8_t>& values)
	{
		// Pairs of (count 1..255, value).
		std::vector<uint8_t> runs;
		size_t i = 0;
		while (i < values.size())
		{
			const uint8_t value = values[i];
			size_t count = 1;
			while (i + count < values.size() && values[i + count] == value && count < 255)
			{
				count++;
			}
			runs.push_back((uint8_t)count);
			runs.push_back(value);
			i += count;
		}
		return runs;
	}

	bool TerrainSerializer::DecodeRunLength(const std::vector<uint8_t>& runs, size_t count, std::vector<uint8_t>& values)
	{
		values.clear();
		values.reserve(count);
		if (runs.size() % 2 != 0)
		{
			return false;
		}
		for (size_t i = 0; i < runs.size(); i += 2)
		{
			if (runs[i] == 0 || values.size() + runs[i] > count)
			{
				return false;
			}
			values.insert(values.end(), runs[i], runs[i + 1]);
		}
		return values.size() == count;
	}

	// The weight map's rows are stored north first in the PNG, so it opens as a map (north up).
	static std::vector<uint8_t> FlipRows(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> flipped(rgba.size());
		const size_t row = (size_t)width * 4;
		for (uint32_t y = 0; y < height; y++)
		{
			std::copy(rgba.begin() + (size_t)y * row, rgba.begin() + (size_t)(y + 1) * row, flipped.begin() + (size_t)(height - 1 - y) * row);
		}
		return flipped;
	}

	bool TerrainSerializer::Serialize(const std::filesystem::path& filepath, const Terrain& terrain)
	{
		const TerrainSettings& settings = terrain.GetSettings();
		std::vector<uint8_t> png;
		const std::vector<uint8_t> north = FlipRows(terrain.GetWeights(), terrain.GetWeightMapWidth(), terrain.GetWeightMapHeight());
		if (ImageWriter::EncodePNG(terrain.GetWeightMapWidth(), terrain.GetWeightMapHeight(), north.data(), png) == false)
		{
			HY_CORE_ERROR("TerrainSerializer: can't encode the weight map of '{}'", filepath.string());
			return false;
		}

		// The terrain's own handle goes first; a new file gets one here.
		const AssetHandle handle = AssetManager::ImportAsset(filepath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		if ((uint64_t)handle != 0)
		{
			out << YAML::Key << AssetMetaSerializer::HandleKey << YAML::Value << (uint64_t)handle;
		}

		out << YAML::Key << "Terrain" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << CurrentVersion;
		out << YAML::Key << "CellSize" << YAML::Value << settings.CellSize;
		out << YAML::Key << "StepHeight" << YAML::Value << settings.StepHeight;
		out << YAML::Key << "Size" << YAML::Value << glm::uvec2(terrain.GetCornerCountX(), terrain.GetCornerCountY());
		out << YAML::Key << "BaseLevel" << YAML::Value << settings.BaseLevel;
		out << YAML::Key << "Seed" << YAML::Value << settings.Seed;
		out << YAML::Key << "SplatResolution" << YAML::Value << settings.SplatResolution;
		out << YAML::Key << "Jitter" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Amount" << YAML::Value << settings.Jitter;
		out << YAML::Key << "Smooth" << YAML::Value << settings.JitterSmooth;
		out << YAML::Key << "Scale" << YAML::Value << settings.JitterScale;
		out << YAML::EndMap;
		out << YAML::Key << "EdgeNoise" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ClusterSize" << YAML::Value << settings.EdgeClusterSize;
		out << YAML::Key << "PixelNoise" << YAML::Value << settings.EdgePixelNoise;
		out << YAML::EndMap;

		out << YAML::Key << "Layers" << YAML::Value << YAML::BeginSeq;
		for (const TerrainLayer& layer : settings.Layers)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Material" << YAML::Value << (uint64_t)layer.Material;
			out << YAML::Key << "EdgeRoughness" << YAML::Value << layer.EdgeRoughness;
			out << YAML::Key << "Fringe" << YAML::Value << (uint64_t)layer.Fringe;
			out << YAML::Key << "FringeLength" << YAML::Value << layer.FringeLength;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::Key << "WallLayers" << YAML::Value << YAML::BeginSeq;
		for (const TerrainWallLayer& wall : settings.WallLayers)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Material" << YAML::Value << (uint64_t)wall.Material;
			out << YAML::Key << "Variation" << YAML::Value << wall.Variation;
			out << YAML::Key << "CapStones" << YAML::Value << wall.CapStones;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::Key << "Lip" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Width" << YAML::Value << settings.LipWidth;
		out << YAML::Key << "FaceHeight" << YAML::Value << settings.LipFaceHeight;
		out << YAML::EndMap;
		out << YAML::Key << "WallProfile" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Lean" << YAML::Value << settings.WallLean;
		out << YAML::Key << "Noise" << YAML::Value << settings.WallNoise;
		out << YAML::EndMap;
		out << YAML::Key << "CapStones" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value << (uint64_t)settings.CapStoneMaterial;
		out << YAML::Key << "Pitch" << YAML::Value << settings.CapPitch;
		out << YAML::Key << "Joint" << YAML::Value << settings.CapJoint;
		out << YAML::Key << "Depth" << YAML::Value << settings.CapDepth;
		out << YAML::Key << "Height" << YAML::Value << settings.CapHeight;
		out << YAML::Key << "SizeJitter" << YAML::Value << settings.CapSizeJitter;
		out << YAML::Key << "PositionJitter" << YAML::Value << settings.CapPositionJitter;
		out << YAML::Key << "YawJitter" << YAML::Value << settings.CapYawJitter;
		out << YAML::Key << "Missing" << YAML::Value << settings.CapMissing;
		out << YAML::EndMap;
		out << YAML::Key << "Decor" << YAML::Value << YAML::BeginSeq;
		for (const TerrainDecorRule& rule : settings.Decor)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Place" << YAML::Value << DecorPlaceToString(rule.Place);
			out << YAML::Key << "Layer" << YAML::Value << rule.Layer;
			out << YAML::Key << "Shape" << YAML::Value << DecorShapeToString(rule.Shape);
			out << YAML::Key << "Density" << YAML::Value << rule.Density;
			out << YAML::Key << "Scale" << YAML::Value << rule.Scale;
			out << YAML::Key << "Sprites" << YAML::Value << YAML::Flow << YAML::BeginSeq;
			for (AssetHandle sprite : rule.Sprites)
			{
				out << (uint64_t)sprite;
			}
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap; // Terrain

		const std::vector<int8_t>& levels = terrain.GetLevels();
		const std::vector<uint8_t> levelBytes(levels.begin(), levels.end());
		out << YAML::Key << "Data" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Levels" << YAML::Value << YAML::Literal << EncodeBase64(EncodeRunLength(levelBytes));
		out << YAML::Key << "Ramps" << YAML::Value << YAML::Literal << EncodeBase64(EncodeRunLength(terrain.GetRamps()));
		out << YAML::Key << "WallMaterial" << YAML::Value << YAML::Literal << EncodeBase64(EncodeRunLength(terrain.GetWallMaterials()));
		out << YAML::Key << "Weights" << YAML::Value << YAML::Literal << EncodeBase64(png);
		out << YAML::EndMap; // Data
		out << YAML::EndMap;

		std::ofstream file(filepath);
		if (file.is_open() == false)
		{
			HY_CORE_ERROR("TerrainSerializer: can't write '{}'", filepath.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	static bool ReadBlock(const YAML::Node& data, const char* key, size_t count, std::vector<uint8_t>& values)
	{
		const YAML::Node node = data[key];
		if (node.IsDefined() == false)
		{
			return false;
		}
		std::vector<uint8_t> runs;
		return TerrainSerializer::DecodeBase64(node.as<std::string>(std::string()), runs) && TerrainSerializer::DecodeRunLength(runs, count, values);
	}

	Ref<Terrain> TerrainSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		YAML::Node root;
		try
		{
			root = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("TerrainSerializer: can't read '{}': {}", filepath.string(), e.what());
			return nullptr;
		}

		const YAML::Node node = root["Terrain"];
		const YAML::Node data = root["Data"];
		if (node.IsDefined() == false || node.IsMap() == false || data.IsDefined() == false || data.IsMap() == false)
		{
			HY_CORE_ERROR("TerrainSerializer: '{}' has no Terrain or Data section", filepath.string());
			return nullptr;
		}

		TerrainSettings settings;
		glm::uvec2 corners{ 2 };
		try
		{
			const uint32_t version = node["Version"].as<uint32_t>(CurrentVersion);
			if (version > CurrentVersion)
			{
				HY_CORE_WARN("TerrainSerializer: '{}' is version {}, newer than this editor ({}); unknown fields are ignored", filepath.string(), version, CurrentVersion);
			}
			settings.CellSize = node["CellSize"].as<float>(settings.CellSize);
			settings.StepHeight = node["StepHeight"].as<float>(settings.StepHeight);
			corners = node["Size"].as<glm::uvec2>(corners);
			settings.BaseLevel = node["BaseLevel"].as<int32_t>(settings.BaseLevel);
			settings.Seed = node["Seed"].as<uint32_t>(settings.Seed);
			settings.SplatResolution = std::max(node["SplatResolution"].as<uint32_t>(settings.SplatResolution), 1u);
			if (auto jitter = node["Jitter"])
			{
				settings.Jitter = jitter["Amount"].as<float>(settings.Jitter);
				settings.JitterSmooth = jitter["Smooth"].as<float>(settings.JitterSmooth);
				settings.JitterScale = jitter["Scale"].as<float>(settings.JitterScale);
			}
			if (auto noise = node["EdgeNoise"])
			{
				settings.EdgeClusterSize = noise["ClusterSize"].as<float>(settings.EdgeClusterSize);
				settings.EdgePixelNoise = noise["PixelNoise"].as<float>(settings.EdgePixelNoise);
			}
			if (auto layers = node["Layers"])
			{
				for (const YAML::Node& layerNode : layers)
				{
					if (settings.Layers.size() == MaxTerrainLayers)
					{
						break;
					}
					TerrainLayer layer;
					layer.Material = AssetHandle(layerNode["Material"].as<uint64_t>(0));
					layer.EdgeRoughness = layerNode["EdgeRoughness"].as<float>(layer.EdgeRoughness);
					layer.Fringe = AssetHandle(layerNode["Fringe"].as<uint64_t>(0));
					layer.FringeLength = layerNode["FringeLength"].as<float>(layer.FringeLength);
					settings.Layers.push_back(layer);
				}
			}
			if (auto walls = node["WallLayers"])
			{
				for (const YAML::Node& wallNode : walls)
				{
					if (settings.WallLayers.size() == MaxTerrainWallLayers)
					{
						break;
					}
					TerrainWallLayer wall;
					wall.Material = AssetHandle(wallNode["Material"].as<uint64_t>(0));
					wall.Variation = wallNode["Variation"].as<float>(wall.Variation);
					wall.CapStones = wallNode["CapStones"].as<bool>(wall.CapStones);
					settings.WallLayers.push_back(wall);
				}
			}
			if (auto lip = node["Lip"])
			{
				settings.LipWidth = lip["Width"].as<float>(settings.LipWidth);
				settings.LipFaceHeight = lip["FaceHeight"].as<float>(settings.LipFaceHeight);
			}
			if (auto profile = node["WallProfile"])
			{
				settings.WallLean = profile["Lean"].as<float>(settings.WallLean);
				settings.WallNoise = profile["Noise"].as<float>(settings.WallNoise);
			}
			if (auto caps = node["CapStones"])
			{
				settings.CapStoneMaterial = AssetHandle(caps["Material"].as<uint64_t>(0));
				settings.CapPitch = caps["Pitch"].as<float>(settings.CapPitch);
				settings.CapJoint = caps["Joint"].as<float>(settings.CapJoint);
				settings.CapDepth = caps["Depth"].as<float>(settings.CapDepth);
				settings.CapHeight = caps["Height"].as<glm::vec2>(settings.CapHeight);
				settings.CapSizeJitter = caps["SizeJitter"].as<float>(settings.CapSizeJitter);
				settings.CapPositionJitter = caps["PositionJitter"].as<float>(settings.CapPositionJitter);
				settings.CapYawJitter = caps["YawJitter"].as<float>(settings.CapYawJitter);
				settings.CapMissing = caps["Missing"].as<float>(settings.CapMissing);
			}
			if (auto decor = node["Decor"])
			{
				for (const auto& ruleNode : decor)
				{
					if (settings.Decor.size() == MaxTerrainDecorRules)
					{
						HY_CORE_WARN("TerrainSerializer: '{}' has more than {} decor rules; the rest are ignored", filepath.string(), MaxTerrainDecorRules);
						break;
					}
					TerrainDecorRule rule;
					rule.Place = DecorPlaceFromString(ruleNode["Place"].as<std::string>("Layer"));
					rule.Layer = ruleNode["Layer"].as<uint32_t>(rule.Layer);
					rule.Shape = DecorShapeFromString(ruleNode["Shape"].as<std::string>("Upright"));
					rule.Density = ruleNode["Density"].as<float>(rule.Density);
					rule.Scale = ruleNode["Scale"].as<glm::vec2>(rule.Scale);
					if (auto sprites = ruleNode["Sprites"])
					{
						for (const auto& sprite : sprites)
						{
							if (rule.Sprites.size() < MaxTerrainDecorSprites)
							{
								rule.Sprites.push_back(AssetHandle(sprite.as<uint64_t>(0)));
							}
						}
					}
					settings.Decor.push_back(rule);
				}
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("TerrainSerializer: bad settings in '{}': {}", filepath.string(), e.what());
			return nullptr;
		}

		if (corners.x < 2 || corners.y < 2 || corners.x > 4097 || corners.y > 4097 || settings.CellSize <= 0.0f || settings.StepHeight <= 0.0f)
		{
			HY_CORE_ERROR("TerrainSerializer: '{}' has a bad size ({} x {} corners, cell {} m, step {} m)", filepath.string(), corners.x, corners.y,
				settings.CellSize, settings.StepHeight);
			return nullptr;
		}

		Ref<Terrain> terrain = CreateRef<Terrain>(settings, corners.x, corners.y);
		const size_t cornerCount = (size_t)corners.x * corners.y;
		const size_t cellCount = (size_t)(corners.x - 1) * (corners.y - 1);
		std::vector<uint8_t> levelBytes;
		std::vector<uint8_t> ramps;
		std::vector<uint8_t> walls;
		std::vector<uint8_t> png;
		bool ok = false;
		try
		{
			ok = ReadBlock(data, "Levels", cornerCount, levelBytes) && ReadBlock(data, "Ramps", cellCount, ramps)
				&& ReadBlock(data, "WallMaterial", cellCount, walls)
				&& DecodeBase64(data["Weights"].as<std::string>(std::string()), png);
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_ERROR("TerrainSerializer: bad data in '{}': {}", filepath.string(), e.what());
			return nullptr;
		}
		if (ok == false)
		{
			HY_CORE_ERROR("TerrainSerializer: the data of '{}' doesn't fit its size ({} x {} corners)", filepath.string(), corners.x, corners.y);
			return nullptr;
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load_from_memory(png.data(), (int)png.size(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			HY_CORE_ERROR("TerrainSerializer: the weight map of '{}' can't be decoded: {}", filepath.string(), stbi_failure_reason());
			return nullptr;
		}
		const std::vector<uint8_t> north(pixels, pixels + (size_t)width * height * 4);
		stbi_image_free(pixels);
		std::vector<uint8_t> weights = FlipRows(north, (uint32_t)width, (uint32_t)height);

		const glm::uvec2 expected = Terrain::GetWeightMapSize(settings, corners.x - 1, corners.y - 1);
		if ((uint32_t)width != expected.x || (uint32_t)height != expected.y)
		{
			HY_CORE_WARN("TerrainSerializer: the weight map of '{}' is {} x {}, {} x {} was expected; it is stretched over the terrain",
				filepath.string(), width, height, expected.x, expected.y);
		}

		std::vector<int8_t> levels(levelBytes.begin(), levelBytes.end());
		if (terrain->SetData(std::move(levels), std::move(ramps), std::move(walls), std::move(weights), (uint32_t)width, (uint32_t)height) == false)
		{
			HY_CORE_ERROR("TerrainSerializer: the data of '{}' doesn't fit its size", filepath.string());
			return nullptr;
		}
		terrain->SetModified(false);
		return terrain;
	}
}
