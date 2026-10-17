#pragma once
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
		// Relative to the asset directory. Empty = the application decides what to open first.
		std::filesystem::path StartScene;

		// Applied with Renderer::SetSettings when the project is opened.
		RendererSettings Renderer;
	};

	// The project that is open: its settings and where its files live. Not a subsystem: it is data
	// that applications open and close (Load/New, Close), and it never calls into other subsystems.
	// Whoever opens a project applies its settings (e.g. Renderer::SetSettings(config.Renderer)).
	class Project
	{
	public:
		static constexpr const char* FileExtension = ".hyproj";

		ProjectConfig& GetConfig() { return m_Config; }
		const ProjectConfig& GetConfig() const { return m_Config; }

		const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }
		const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }
		std::filesystem::path GetAssetDirectory() const { return m_ProjectDirectory / m_Config.AssetDirectory; }
		// Absolute path of the start scene, or empty when none is set.
		std::filesystem::path GetStartScenePath() const;

		static Ref<Project> GetActive() { return s_ActiveProject; }

		// Makes a new, unsaved project active. `projectFile` is where SaveActive will write it
		// (<directory>/<Name>.hyproj); its folder is the project directory.
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
