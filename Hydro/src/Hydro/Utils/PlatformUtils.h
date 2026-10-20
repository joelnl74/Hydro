#pragma once
#include <string>
#include <vector>

namespace Hydro
{
	// Native open/save dialogs. One implementation per OS: Platform/Windows/WindowsFileDialogs.cpp,
	// Platform/MacOS/MacOSFileDialogs.cpp, Platform/Linux/LinuxFileDialogs.cpp.
	//
	// `filter` uses the Windows format on every platform: pairs of NUL-terminated strings,
	// "Description\0pattern;pattern\0", e.g. "Hydro Scene (*.hyscene)\0*.hyscene\0".
	class FileDialogs
	{
	public:
		// These return an empty string if cancelled.
		static std::string OpenFile(const char* filter);
		static std::string OpenDirectory();
		static std::string SaveFile(const char* filter);

		// The extensions in `filter`, without "*." ("hyscene"). Patterns that aren't "*.ext" are skipped.
		static std::vector<std::string> GetFilterExtensions(const char* filter);
		// The first description in `filter` ("Hydro Scene (*.hyscene)").
		static std::string GetFilterDescription(const char* filter);
	};
}
