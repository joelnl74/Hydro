#include "hypch.h"
#include "EngineResources.h"

#include "Hydro/Utils/PlatformUtils.h"

namespace Hydro
{
	struct EngineResourcesData
	{
		std::vector<std::filesystem::path> DevelopmentDirectories;
		std::filesystem::path StagedDirectory;
	};

	static EngineResourcesData* s_Data = nullptr;

	void EngineResources::Init(const EngineResourcesSpecification& specification)
	{
		HY_CORE_ASSERT(s_Data == nullptr, "EngineResources::Init called twice");
		s_Data = new EngineResourcesData();
		s_Data->StagedDirectory = Process::GetExecutablePath().parent_path() / "assets";

		std::error_code ec;
		for (const std::filesystem::path& directory : specification.DevelopmentDirectories)
		{
			if (std::filesystem::is_directory(directory / "assets", ec))
			{
				s_Data->DevelopmentDirectories.push_back(directory / "assets");
				HY_CORE_INFO("Engine resources: using {} before the copy next to the exe", (directory / "assets").string());
			}
		}
	}

	void EngineResources::Shutdown()
	{
		delete s_Data;
		s_Data = nullptr;
	}

	std::filesystem::path EngineResources::Resolve(const std::filesystem::path& relativePath)
	{
		HY_CORE_ASSERT(s_Data != nullptr, "EngineResources::Resolve before Init");
		if (relativePath.is_absolute())
		{
			return relativePath;
		}

		std::error_code ec;
		for (const std::filesystem::path& directory : s_Data->DevelopmentDirectories)
		{
			const std::filesystem::path candidate = (directory / relativePath).lexically_normal();
			if (std::filesystem::exists(candidate, ec))
			{
				return candidate;
			}
		}

		const std::filesystem::path staged = (s_Data->StagedDirectory / relativePath).lexically_normal();
		if (std::filesystem::exists(staged, ec) == false)
		{
			HY_CORE_ERROR("Engine resource '{}' not found next to the exe ({}){}", relativePath.generic_string(),
				staged.string(), s_Data->DevelopmentDirectories.empty() ? "" : " or in the development folders");
		}
		return staged;
	}

	std::filesystem::path EngineResources::GetStagedDirectory()
	{
		HY_CORE_ASSERT(s_Data != nullptr, "EngineResources::GetStagedDirectory before Init");
		return s_Data->StagedDirectory;
	}
}
