#pragma once
#include <filesystem>
#include <string>

#include "Hydro/Core/Base.h"

namespace Hydro {

	class FileDialogs
	{
	public:
		// These return empty strings if cancelled.
		static std::string OpenFile(const char* filter);
		static std::string OpenDirectory();
		static std::string SaveFile(const char* filter);
	};

	// The command line Process starts (program first, quoted when its path has spaces) and its folder (empty: ours).
	struct ProcessSpecification
	{
		std::string CommandLine;
		std::filesystem::path WorkingDirectory;
	};

	struct ProcessData;

	// A program the application starts and watches (a build script): its output is collected, and Kill or the
	// destructor stops it and everything it started. Windows only for now.
	class Process
	{
	public:
		explicit Process(const ProcessSpecification& specification);
		~Process();

		Process(const Process&) = delete;
		Process& operator=(const Process&) = delete;

		bool IsValid() const;
		bool IsRunning() const;
		// The exit code once it has finished; -1 while it runs or when it never started.
		int GetExitCode() const;
		// Stops it and every program it started. Nothing happens when it has finished.
		void Kill();
		// The output that arrived since the last call, as the program wrote it.
		std::string ReadOutput();

		// Starts a program that runs on its own (a game, Explorer): nothing collected, never killed. False (logged) on failure.
		static bool Launch(const ProcessSpecification& specification);
		// The running program's own executable file.
		static std::filesystem::path GetExecutablePath();

	private:
		Scope<ProcessData> m_Data;
	};
}
