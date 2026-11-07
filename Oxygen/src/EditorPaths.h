#pragma once
#include "Hydro.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace Hydro
{
	// Where the editor saves scenes and prefabs: the active project's folders, else the "project" folder next to the
	// working directory.
	class EditorPaths
	{
	public:
		// Per user, not per repository or project: %APPDATA%/Hydro/Oxygen (the editor settings, imgui.ini).
		static std::filesystem::path GetUserDirectory()
		{
			if (const char* appData = std::getenv("APPDATA"); appData != nullptr && appData[0] != '\0')
			{
				return std::filesystem::path(appData) / "Hydro" / "Oxygen";
			}
			if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
			{
				return std::filesystem::path(home) / ".config" / "Hydro" / "Oxygen";
			}
			return std::filesystem::current_path() / ".hydro-user";
		}

		// The editor's own settings (theme, colours, sizes, the session): per user.
		static std::filesystem::path GetEditorSettingsPath() { return GetUserDirectory() / "EditorSettings.yaml"; }
		// ImGui's window positions and docking: per user.
		static std::filesystem::path GetImGuiIniPath() { return GetUserDirectory() / "imgui.ini"; }

		// Makes the user folder and, once, copies the files older editors kept next to the working directory (Oxygen/)
		// into it. Runs in CreateApplication, before the log exists: never logs.
		static void PrepareUserDirectory(const std::filesystem::path& legacyDirectory)
		{
			std::error_code ec;
			std::filesystem::create_directories(GetUserDirectory(), ec);
			for (const std::filesystem::path& target : { GetEditorSettingsPath(), GetImGuiIniPath() })
			{
				const std::filesystem::path legacy = legacyDirectory / target.filename();
				if (std::filesystem::exists(target, ec) == false && std::filesystem::exists(legacy, ec))
				{
					std::filesystem::copy_file(legacy, target, ec);
				}
			}
		}

		// What the editor remembers about one project (its last scene): <project>/.hydro/Editor.yaml, not in git.
		static std::filesystem::path GetProjectEditorStatePath() { return GetProjectDirectory() / ".hydro" / "Editor.yaml"; }

		// The project the editor opens at startup: the first .hyproj in this folder.
		static std::filesystem::path GetDefaultProjectDirectory()
		{
			return std::filesystem::current_path() / "project";
		}

		static std::filesystem::path GetProjectDirectory()
		{
			if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
			{
				return project->GetProjectDirectory();
			}

			return GetDefaultProjectDirectory();
		}

		static std::filesystem::path GetAssetsDirectory()
		{
			if (auto project = Project::GetActive(); project != nullptr && project->GetProjectDirectory().empty() == false)
			{
				return project->GetAssetDirectory();
			}

			return GetDefaultProjectDirectory() / "Assets";
		}

		static std::filesystem::path GetScenesDirectory() { return GetAssetsDirectory() / "Scenes"; }

		// View captures (Capture menu, F9): <project>/.hydro/captures. A cache folder, not in git.
		static std::filesystem::path GetCapturesDirectory() { return GetProjectDirectory() / ".hydro" / "captures"; }
		static std::filesystem::path GetPrefabsDirectory() { return GetAssetsDirectory() / "Prefabs"; }

		// Turns an entity / scene name into something safe to use as a file name.
		static std::string ToFileName(const std::string& name)
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
		static std::filesystem::path GetPrefabPath(const std::string& name)
		{
			return GetPrefabsDirectory() / (ToFileName(name) + PrefabSerializer::FileExtension);
		}
	};
}
