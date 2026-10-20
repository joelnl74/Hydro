#include "hypch.h"
#include "Hydro/Utils/PlatformUtils.h"

#include <cctype>
#include <cstring>

namespace Hydro
{
	std::vector<std::string> FileDialogs::GetFilterExtensions(const char* filter)
	{
		std::vector<std::string> extensions;
		if (filter == nullptr || *filter == '\0')
		{
			return extensions;
		}

		// Skip each description; read each pattern list ("*.png;*.jpg").
		const char* entry = filter;
		while (*entry != '\0')
		{
			const char* patterns = entry + strlen(entry) + 1;
			if (*patterns == '\0')
			{
				break;
			}

			std::string pattern;
			for (const char* c = patterns; ; c++)
			{
				if (*c == ';' || *c == '\0')
				{
					if (pattern.size() > 2 && pattern[0] == '*' && pattern[1] == '.')
					{
						std::string extension = pattern.substr(2);
						const bool plain = std::all_of(extension.begin(), extension.end(), [](char ch) { return std::isalnum((unsigned char)ch) != 0; });
						if (plain)
						{
							extensions.push_back(extension);
						}
					}
					pattern.clear();

					if (*c == '\0')
					{
						break;
					}
				}
				else if (*c != ' ')
				{
					pattern += *c;
				}
			}

			entry = patterns + strlen(patterns) + 1;
		}

		return extensions;
	}

	std::string FileDialogs::GetFilterDescription(const char* filter)
	{
		if (filter == nullptr)
		{
			return std::string();
		}

		return std::string(filter);
	}
}
