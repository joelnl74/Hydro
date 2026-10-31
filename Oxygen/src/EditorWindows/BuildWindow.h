#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include <Hydro/Core/Base.h>
#include <Hydro/Utils/PlatformUtils.h>

namespace Hydro
{
	// "Build" window (File > Build...): makes a standalone build of the open project. A thin UI over
	// scripts/Package.bat, which builds the Runtime (Release or Dist), names it after the project and copies
	// the project's files into Builds/<Project>-<Kind>/; the window shows the script's output while it runs
	// and runs the game or opens its folder when it is done. The build runs in the background: the editor
	// stays usable, and a build still running when the editor closes is stopped.
	class BuildWindow
	{
	public:
		void OnImguiRender();

		bool& IsOpen() { return m_Open; }

		// Called just before a build starts. The build packages what is on disk, so the editor saves the
		// project and the open scene here.
		std::function<void()> OnBeforeBuild;

	private:
		enum class BuildKind : uint8_t { Release, Distribution };
		enum class BuildState : uint8_t { Idle, Building, Succeeded, Failed, Cancelled };

		void StartBuild();
		void UpdateBuild();
		void RunGame();
		void OpenFolder();

		// The project's file name without extension: the exe and the Builds folder are named after it.
		std::string GetProjectName() const;
		std::filesystem::path GetOutputDirectory() const;
		static const char* GetKindName(BuildKind kind);

	private:
		bool m_Open = false;
		BuildKind m_Kind = BuildKind::Release;
		BuildState m_State = BuildState::Idle;

		// The repository (where scripts/Package.bat is), found when the window first opens.
		std::filesystem::path m_RepositoryRoot;
		bool m_RepositorySearched = false;

		Scope<Process> m_Process;
		std::string m_Log;
		bool m_ScrollToEnd = false;
		double m_StartTime = 0.0;
		double m_Duration = 0.0;
		int m_ExitCode = 0;
		// What the last build made (the kind can change after it finished).
		std::filesystem::path m_BuiltDirectory;
		std::string m_BuiltName;
	};
}
