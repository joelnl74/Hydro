#include "RecentProjects.h"

#include <Hydro/Core/Log.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <ctime>
#include <fstream>

namespace Hydro
{
	ProjectCompatibility RecentProject::GetCompatibility() const
	{
		if (Game.IsGeneric())
		{
			return ProjectCompatibility::Generic;
		}
		return Game.Id == Project::GetCompiledGame().Id ? ProjectCompatibility::Compatible : ProjectCompatibility::OtherGame;
	}

	static std::string ReadString(const YAML::Node& node, const char* key)
	{
		if (auto child = node[key])
		{
			return child.as<std::string>(std::string());
		}
		return std::string();
	}

	void RecentProjects::Load(const std::filesystem::path& path)
	{
		m_Entries.clear();
		std::error_code ec;
		if (std::filesystem::exists(path, ec) == false)
		{
			return;
		}

		try
		{
			const YAML::Node root = YAML::LoadFile(path.string());
			const YAML::Node projects = root["RecentProjects"];
			if (projects.IsDefined() == false || projects.IsSequence() == false)
			{
				return;
			}
			for (const YAML::Node& node : projects)
			{
				RecentProject project;
				project.Path = ReadString(node, "Path");
				project.Name = ReadString(node, "Name");
				project.Game.Id = ReadString(node, "GameId");
				project.Game.Name = ReadString(node, "GameName");
				project.LastOpened = ReadString(node, "LastOpened");
				project.Editor = ReadString(node, "Editor");
				if (project.Path.empty() == false && m_Entries.size() < MaxCount)
				{
					m_Entries.push_back(project);
				}
			}
		}
		catch (const YAML::Exception& e)
		{
			HY_WARN("Could not read the recent projects {}: {}", path.string(), e.what());
			m_Entries.clear();
		}
	}

	bool RecentProjects::Save(const std::filesystem::path& path) const
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
		for (const RecentProject& project : m_Entries)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Path" << YAML::Value << project.Path.generic_string();
			out << YAML::Key << "Name" << YAML::Value << project.Name;
			out << YAML::Key << "GameId" << YAML::Value << project.Game.Id;
			out << YAML::Key << "GameName" << YAML::Value << project.Game.Name;
			out << YAML::Key << "LastOpened" << YAML::Value << project.LastOpened;
			out << YAML::Key << "Editor" << YAML::Value << project.Editor.generic_string();
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream file(path);
		if (file.is_open() == false)
		{
			HY_WARN("Could not write the recent projects to {}", path.string());
			return false;
		}
		file << out.c_str();
		return true;
	}

	void RecentProjects::Add(RecentProject project)
	{
		project.LastOpened = Now();
		Remove(project.Path);
		m_Entries.insert(m_Entries.begin(), project);
		if (m_Entries.size() > MaxCount)
		{
			m_Entries.resize(MaxCount);
		}
	}

	void RecentProjects::Remove(const std::filesystem::path& path)
	{
		const std::filesystem::path normal = path.lexically_normal();
		m_Entries.erase(std::remove_if(m_Entries.begin(), m_Entries.end(),
			[&normal](const RecentProject& entry) { return entry.Path.lexically_normal() == normal; }), m_Entries.end());
	}

	std::string RecentProjects::Now()
	{
		const std::time_t now = std::time(nullptr);
		std::tm local{};
#if defined(_WIN32)
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		char text[32] = {};
		std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local);
		return text;
	}
}
