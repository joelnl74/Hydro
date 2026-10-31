#include "hypch.h"
#include "AssetMetaSerializer.h"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace Hydro
{
	// True for lines that come before a YAML file's first key: blank lines, comments, "---" and "%" directives.
	static bool IsPreambleLine(const std::string& line)
	{
		size_t first = line.find_first_not_of(" \t\r");
		if (first == std::string::npos)
		{
			return true;
		}

		return line[first] == '#' || line[first] == '%' || line.compare(0, 3, "---") == 0;
	}

	// "Handle: 123" -> 123; 0 when `line` is not a handle line.
	static AssetHandle ParseHandleLine(const std::string& line)
	{
		const std::string prefix = std::string(AssetMetaSerializer::HandleKey) + ":";
		if (line.compare(0, prefix.size(), prefix) != 0)
		{
			return 0;
		}

		std::string value = line.substr(prefix.size());
		const size_t first = value.find_first_not_of(" \t");
		const size_t last = value.find_last_not_of(" \t\r");
		if (first == std::string::npos)
		{
			return 0;
		}
		value = value.substr(first, last - first + 1);

		for (char c : value)
		{
			if (std::isdigit((unsigned char)c) == 0)
			{
				return 0;
			}
		}

		try
		{
			return AssetHandle(std::stoull(value));
		}
		catch (const std::exception&)
		{
			return 0;
		}
	}

	static bool ReadWholeFile(const std::filesystem::path& file, std::string& contents)
	{
		std::ifstream in(file, std::ios::binary);
		if (in.is_open() == false)
		{
			return false;
		}

		contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		return true;
	}

	bool AssetMetaSerializer::StoresHandleInFile(AssetType type)
	{
		return type == AssetType::Scene || type == AssetType::Prefab || type == AssetType::SpriteAnimation || type == AssetType::Material
			|| type == AssetType::RenderProfile || type == AssetType::Terrain;
	}

	std::filesystem::path AssetMetaSerializer::GetSidecarPath(const std::filesystem::path& file)
	{
		std::filesystem::path sidecar = file;
		sidecar += FileExtension;
		return sidecar;
	}

	bool AssetMetaSerializer::IsSidecar(const std::filesystem::path& path)
	{
		return path.extension().string() == FileExtension;
	}

	AssetHandle AssetMetaSerializer::ReadHandle(const std::filesystem::path& file, AssetType type)
	{
		if (StoresHandleInFile(type))
		{
			// Only the first key: Hydro writes the handle first, so the rest (every entity) isn't parsed.
			std::ifstream in(file, std::ios::binary);
			std::string line;
			while (std::getline(in, line))
			{
				if (IsPreambleLine(line))
				{
					continue;
				}

				return ParseHandleLine(line);
			}

			return 0;
		}

		const std::filesystem::path sidecar = GetSidecarPath(file);
		std::error_code ec;
		if (std::filesystem::exists(sidecar, ec) == false)
		{
			return 0;
		}

		try
		{
			YAML::Node data = YAML::LoadFile(sidecar.string());
			if (auto handle = data[HandleKey])
			{
				return AssetHandle(handle.as<uint64_t>(0));
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_CORE_WARN("AssetMetaSerializer: can't read '{}': {}", sidecar.string(), e.what());
		}

		return 0;
	}

	bool AssetMetaSerializer::WriteHandle(const std::filesystem::path& file, AssetType type, AssetHandle handle)
	{
		if (StoresHandleInFile(type))
		{
			std::string contents;
			if (ReadWholeFile(file, contents) == false)
			{
				HY_CORE_ERROR("AssetMetaSerializer: can't read '{}' to add its asset handle", file.string());
				return false;
			}

			// Keep the file's line endings (CRLF on Windows).
			const std::string newline = contents.find("\r\n") != std::string::npos ? "\r\n" : "\n";
			const std::string handleLine = std::string(HandleKey) + ": " + std::to_string((uint64_t)handle);

			// Find the first key line: replace it if it is the handle, otherwise put the handle above it.
			size_t position = 0;
			while (position < contents.size())
			{
				size_t end = contents.find('\n', position);
				if (end == std::string::npos)
				{
					end = contents.size();
				}

				const std::string line = contents.substr(position, end - position);
				if (IsPreambleLine(line) == false)
				{
					if ((uint64_t)ParseHandleLine(line) != 0)
					{
						size_t lineEnd = end;
						if (lineEnd > position && contents[lineEnd - 1] == '\r')
						{
							lineEnd--;
						}
						contents.replace(position, lineEnd - position, handleLine);
					}
					else
					{
						contents.insert(position, handleLine + newline);
					}
					break;
				}

				position = end + 1;
			}

			if (position >= contents.size())
			{
				contents += handleLine + newline;
			}

			std::ofstream out(file, std::ios::binary | std::ios::trunc);
			if (out.is_open() == false)
			{
				HY_CORE_ERROR("AssetMetaSerializer: can't write the asset handle into '{}'", file.string());
				return false;
			}

			out << contents;
			return true;
		}

		// A sidecar may already hold other settings (texture import settings): keep them, handle and type first.
		const std::filesystem::path sidecar = GetSidecarPath(file);
		YAML::Node existing;
		std::error_code ec;
		if (std::filesystem::exists(sidecar, ec))
		{
			try
			{
				existing = YAML::LoadFile(sidecar.string());
			}
			catch (const YAML::Exception& e)
			{
				HY_CORE_WARN("AssetMetaSerializer: '{}' is not valid YAML ({}); rewriting it", sidecar.string(), e.what());
			}
		}

		YAML::Emitter emitter;
		emitter << YAML::BeginMap;
		emitter << YAML::Key << HandleKey << YAML::Value << (uint64_t)handle;
		emitter << YAML::Key << "Type" << YAML::Value << AssetTypeToString(type);
		if (existing.IsMap())
		{
			for (const auto& entry : existing)
			{
				const std::string key = entry.first.as<std::string>("");
				if (key != HandleKey && key != "Type")
				{
					emitter << YAML::Key << entry.first << YAML::Value << entry.second;
				}
			}
		}
		emitter << YAML::EndMap;

		std::ofstream out(sidecar);
		if (out.is_open() == false)
		{
			HY_CORE_ERROR("AssetMetaSerializer: can't write '{}'", sidecar.string());
			return false;
		}

		out << emitter.c_str() << "\n";
		return true;
	}
}
