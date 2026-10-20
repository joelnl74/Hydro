#include "hypch.h"
#include "Hydro/Utils/PlatformUtils.h"

#include <cstdio>
#include <filesystem>

#include <sys/wait.h>

// Linux file dialogs through zenity (GTK). Linux is built for checks (scripts/check.sh, the Linux
// build), not a supported target yet; without zenity the dialogs log an error and return "".
namespace Hydro
{
	// Runs `command` and returns the first line it prints, without the newline.
	static std::string RunDialog(const std::string& command)
	{
		FILE* pipe = popen((command + " 2>/dev/null").c_str(), "r");
		if (pipe == nullptr)
		{
			HY_CORE_ERROR("FileDialogs: could not run zenity");
			return std::string();
		}

		std::string output;
		char buffer[1024];
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			output += buffer;
		}

		const int status = pclose(pipe);
		if (status != 0 && output.empty())
		{
			// 1 = cancelled; 127 = zenity is not installed.
			if (WEXITSTATUS(status) == 127)
			{
				HY_CORE_ERROR("FileDialogs: zenity is not installed");
			}
			return std::string();
		}

		while (output.empty() == false && (output.back() == '\n' || output.back() == '\r'))
		{
			output.pop_back();
		}

		return output;
	}

	static std::string ZenityFilter(const char* filter)
	{
		const std::vector<std::string> extensions = FileDialogs::GetFilterExtensions(filter);
		if (extensions.empty())
		{
			return std::string();
		}

		std::string patterns;
		for (const std::string& extension : extensions)
		{
			patterns += " *." + extension;
		}

		return " --file-filter='" + FileDialogs::GetFilterDescription(filter) + " |" + patterns + "'";
	}

	std::string FileDialogs::OpenFile(const char* filter)
	{
		return RunDialog("zenity --file-selection --title='Open'" + ZenityFilter(filter));
	}

	std::string FileDialogs::OpenDirectory()
	{
		return RunDialog("zenity --file-selection --directory --title='Select folder for project creation'");
	}

	std::string FileDialogs::SaveFile(const char* filter)
	{
		std::string path = RunDialog("zenity --file-selection --save --confirm-overwrite --title='Save'" + ZenityFilter(filter));

		const std::vector<std::string> extensions = GetFilterExtensions(filter);
		if (path.empty() == false && extensions.empty() == false && std::filesystem::path(path).has_extension() == false)
		{
			path += "." + extensions.front();
		}

		return path;
	}
}
