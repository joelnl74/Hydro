#pragma once
#include "Hydro/Asset/Asset.h"
#include "Hydro/Core/Base.h"
#include "Hydro/Renderer/RendererSettings.h"

#include <string>
#include <filesystem>

namespace Hydro
{
	// Everything saved in a .hyproj file (YAML, see ProjectSerializer).
	struct ProjectConfig
	{
		std::string Name = "Untitled";

		// Relative to the project directory (the folder that holds the .hyproj file).
		std::filesystem::path AssetDirectory = "Assets";
		// The scene to open first, by handle. 0 = the application decides.
		AssetHandle StartScene = 0;
		// An older project file's start scene path (relative to the asset directory). Whoever opens the project turns it
		// into `StartScene` once the registry exists; written back only while `StartScene` is 0.
		std::filesystem::path LegacyStartScene;
		// The scene New Scene starts as a copy of, by handle. 0 = a new scene has only a camera.
		AssetHandle SceneTemplate = 0;

		// Applied with Renderer::SetSettings when the project is opened.
		RendererSettings Renderer;
	};

	// The open project: its settings and where its files live. Data, not a subsystem: whoever opens it applies its
	// settings (e.g. Renderer::SetSettings).
	class Project
	{
	public:
		static constexpr const char* FileExtension = ".hyproj";

		ProjectConfig& GetConfig() { return m_Config; }
		const ProjectConfig& GetConfig() const { return m_Config; }

		const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
		const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
		std::filesystem::path GetAssetDirectory() const { return m_ProjectDirectory / m_Config.AssetDirectory; }

		static Ref<Project> GetActive() { return s_ActiveProject; }

		// Makes a new, unsaved project active; SaveActive writes it to `projectFile`, whose folder is the project's.
		static Ref<Project> New(const std::filesystem::path& projectFile);
		// Loads a .hyproj and makes it active. On failure, returns nullptr and keeps the current project.
		static Ref<Project> Load(const std::filesystem::path& projectFile);
		// Writes the active project to its file.
		static bool SaveActive();
		// Releases the active project. Applications call this when they shut down.
		static void Close();

	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;
		std::filesystem::path m_ProjectFilePath;

		inline static Ref<Project> s_ActiveProject;
	};
}
