#include "hypch.h"
#include "Hydro/Utils/PlatformUtils.h"

#include <cstdio>
#include <filesystem>

// macOS file dialogs through AppleScript (osascript), so the engine needs no Objective-C. The
// dialog is the system's own NSOpenPanel / NSSavePanel. Cancelling makes osascript fail with
// "User canceled" on stderr and print nothing, which returns an empty string.
namespace Hydro
{
	// Runs `script` with osascript and returns the first line it prints, without the newline.
	static std::string RunAppleScript(const std::string& script)
	{
		// Single quotes go to the shell as '\'' ; the script itself only uses double quotes.
		std::string quoted;
		for (char c : script)
		{
			if (c == '\'')
			{
				quoted += "'\\''";
			}
			else
			{
				quoted += c;
			}
		}

		const std::string command = "osascript -e '" + quoted + "' 2>/dev/null";
		FILE* pipe = popen(command.c_str(), "r");
		if (pipe == nullptr)
		{
			HY_CORE_ERROR("FileDialogs: could not run osascript");
			return std::string();
		}

		std::string output;
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			output += buffer;
		}
		pclose(pipe);

		while (output.empty() == false && (output.back() == '\n' || output.back() == '\r'))
		{
			output.pop_back();
		}

		return output;
	}

	static std::string AppleScriptList(const std::vector<std::string>& items)
	{
		std::string list = "{";
		for (size_t i = 0; i < items.size(); i++)
		{
			list += (i == 0 ? "\"" : ", \"") + items[i] + "\"";
		}
		return list + "}";
	}

	std::string FileDialogs::OpenFile(const char* filter)
	{
		const std::vector<std::string> extensions = GetFilterExtensions(filter);

		std::string script = "POSIX path of (choose file with prompt \"Open\"";
		if (extensions.empty() == false)
		{
			script += " of type " + AppleScriptList(extensions);
		}
		script += ")";

		return RunAppleScript(script);
	}

	std::string FileDialogs::OpenDirectory()
	{
		std::string path = RunAppleScript("POSIX path of (choose folder with prompt \"Select folder for project creation\")");

		// Folders come back with a trailing '/', unlike on Windows.
		if (path.size() > 1 && path.back() == '/')
		{
			path.pop_back();
		}

		return path;
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		const std::vector<std::string> extensions = GetFilterExtensions(filter);
		const std::string extension = extensions.empty() ? std::string() : extensions.front();

		std::string script = "POSIX path of (choose file name with prompt \"Save\"";
		if (extension.empty() == false)
		{
			script += " default name \"Untitled." + extension + "\"";
		}
		script += ")";

		std::string path = RunAppleScript(script);

		// The save panel doesn't add the extension when the user types a name without one.
		if (path.empty() == false && extension.empty() == false && std::filesystem::path(path).has_extension() == false)
		{
			path += "." + extension;
		}

		return path;
	}
}
