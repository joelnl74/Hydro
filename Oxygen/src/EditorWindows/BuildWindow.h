#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include <Hydro/Core/Base.h>
#include <Hydro/Utils/PlatformUtils.h>

namespace Hydro
{
	// The "Build" window: runs scripts/Package.bat in the background to make a standalone build of the open project,
	// shows its output, and runs the game or opens its folder when it is done.
	class BuildWindow
	{
	public:
		void OnImguiRender();

		bool& IsOpen() { return m_Open; }

		// Called before a build starts, so the editor can save the project and the open scene (the build packages what is on disk).
		std::function<void()> OnBeforeBuild;

	private:
		enum class BuildKind : uint8_t { Release, Distribution };
		enum class BuildState : uint8_t { Idle, Building, Succeeded, Failed, Cancelled };

		void StartBuild();
		void UpdateBuild();
		void RunGame();
		void OpenFolder();

		// The project's file name without extension; the exe and the Builds folder are named after it.
		std::string GetProjectName() const;
		std::filesystem::path GetOutputDirectory() const;
		static const char* GetKindName(BuildKind kind);

	private:
		bool m_Open = false;
		BuildKind m_Kind = BuildKind::Release;
		BuildState m_State = BuildState::Idle;

		// The engine checkout (scripts/Package.bat) and the workspace whose Runtime is packaged (Builds/ goes there): both
		// baked in by premake (HY_HYDRO_ROOT, HY_WORKSPACE_DIR), so nothing depends on the working directory.
		std::filesystem::path m_EngineRoot;
		std::filesystem::path m_WorkspaceRoot;
		bool m_RootsFound = false;

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
