#pragma once
#include <filesystem>
#include <string>

#include "Hydro/Core/Base.h"

namespace Hydro {

	class FileDialogs
	{
	public:
		// These return empty strings if cancelled
		static std::string OpenFile(const char* filter);
		static std::string OpenDirectory();
		static std::string SaveFile(const char* filter);
	};

	// What Process starts: the whole command line (the program first, quoted when its path has spaces, then
	// its arguments) and the folder it runs in (empty: the application's working directory).
	struct ProcessSpecification
	{
		std::string CommandLine;
		std::filesystem::path WorkingDirectory;
	};

	struct ProcessData;

	// A program the application starts and keeps an eye on, such as a build script. Starts in the constructor
	// (IsValid says whether it did); its output (stdout and stderr together) is collected while it runs, and Kill
	// stops it with everything it started (a script, the compiler it runs...). Destroying a Process that still
	// runs kills it. For a program that should keep running on its own (a game, Explorer), use Launch.
	// Windows only for now (Platform/Windows/WindowsProcess.cpp).
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

		// Starts a program that runs on its own, in its own console when it has one; nothing is collected and
		// it is never killed. False (logged) when it could not be started.
		static bool Launch(const ProcessSpecification& specification);
		// The running program's own executable file.
		static std::filesystem::path GetExecutablePath();

	private:
		Scope<ProcessData> m_Data;
	};
}
