#pragma once
#include "Hydro.h"

#include <filesystem>
#include <string>

namespace Hydro
{
	// Where the editor saves scenes and prefabs.
	// Uses the active project's directories when a project is open, otherwise the "project" folder
	// next to the editor's working directory (Oxygen/project when running from Visual Studio).
	namespace EditorPaths
	{
		// The project the editor opens at startup: the first .hyproj in this folder.
		inline std::filesystem::path GetDefaultProjectDirectory()
		{
			return std::filesystem::current_path() / "project";
		}

		inline std::filesystem::path GetProjectDirectory()
		{
			if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
				return project->GetProjectDirectory();

			return GetDefaultProjectDirectory();
		}

		inline std::filesystem::path GetAssetsDirectory()
		{
			if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
				return project->GetAssetDirectory();

			return GetDefaultProjectDirectory() / "Assets";
		}

		inline std::filesystem::path GetScenesDirectory() { return GetAssetsDirectory() / "Scenes"; }
		inline std::filesystem::path GetPrefabsDirectory() { return GetAssetsDirectory() / "Prefabs"; }

		// Turns an entity / scene name into something safe to use as a file name.
		inline std::string ToFileName(const std::string& name)
		{
			std::string result;
			for (char c : name)
			{
				const bool invalid = c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
					c == '\\' || c == '|' || c == '?' || c == '*' || (unsigned char)c < 32;
				result += invalid ? '_' : c;
			}

			return result.empty() ? "Untitled" : result;
		}

		// <project>/Assets/Prefabs/<name>.hyprefab
		inline std::filesystem::path GetPrefabPath(const std::string& name)
		{
			return GetPrefabsDirectory() / (ToFileName(name) + PrefabSerializer::FileExtension);
		}
	}
}
